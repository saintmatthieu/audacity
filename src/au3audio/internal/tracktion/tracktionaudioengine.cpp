/*
* Audacity: A Digital Audio Editor
*/
#include "tracktionaudioengine.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

// Qt's keyword macros clash with identifiers in tracktion_graph; this file doesn't use them
#undef slots
#undef signals
#undef emit
#include <juce_audio_devices/juce_audio_devices.h>
#include <tracktion_graph/tracktion_graph.h>

#include "framework/global/containers.h"
#include "framework/global/log.h"

#include "au3-mixer/AudioIOSequences.h"
#include "au3-mixer/Mix.h"
#include "au3-project/Project.h"
#include "au3-realtime-effects/RealtimeEffectList.h"
#include "au3-realtime-effects/RealtimeEffectManager.h"
#include "au3-realtime-effects/RealtimeEffectState.h"
#include "au3-time-frequency-selection/ViewInfo.h"
#include "au3-track/Track.h"
#include "au3-wave-track/WaveTrack.h"

#include "au3wrap/internal/wxtypes_convert.h"

#include "juceaudiodevices.h"
#include "playheadsegment.h"

using namespace au::au3audio;
using namespace tracktion::graph;

namespace {
//! The graph and the device are stereo
constexpr int numChannels = 2;

//! How far right of the loop region the play head may be for loop play to still capture it (as in DefaultPlaybackPolicy)
constexpr double loopCaptureAllowance = 0.5;

using Clock = std::chrono::steady_clock;

//! Per-callback state shared by the nodes; written by the device callback before processing the graph
struct RenderContext {
    RealtimeEffects::ProcessingScope* effects = nullptr;
    Clock::time_point dacTime;
    bool hasSoloingSequence = false;
    //! Where the sequences go silent, unless looping
    std::optional<int64_t> endPosition;
    //! Constant during a stream
    au::audio::IAudioMeterPtr meter;
    //! Of the whole graph, in samples
    int latency = 0;
};

Clock::duration toDuration(int64_t samples, double sampleRate)
{
    return std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(samples / sampleRate));
}

//! The latency, in samples, of the enabled realtime effects of a group (MasterGroup for the master effects)
int effectsLatency(const AudacityProject& project, const ChannelGroup* group, double sampleRate)
{
    const auto& list = group ? RealtimeEffectList::Get(*group) : RealtimeEffectList::Get(project);
    EffectInstance::SampleCount latency = 0;
    list.Visit([&](const RealtimeEffectState& state, bool listIsActive) {
        if (!listIsActive || !state.IsEnabled()) {
            return;
        }
        if (const auto instance = state.PeekInstance()) {
            latency += instance->GetLatency(state.GetSettings(), sampleRate);
        }
    });
    return static_cast<int>(latency);
}

//! The timeline positions of the frames still in the graph's latency, oldest first
class PendingFrames
{
public:
    //! `frames` frames from `start`, or past the end of the play range if not `valid`
    struct Range {
        int64_t start = 0;
        int64_t frames = 0;
        bool valid = true;
    };

    void reset()
    {
        m_ranges.clear();
        m_ranges.reserve(capacity);
    }

    void push(Range range)
    {
        if (range.frames <= 0) {
            return;
        }
        if (!m_ranges.empty()) {
            auto& last = m_ranges.back();
            const bool contiguous = last.valid == range.valid && (!range.valid || last.start + last.frames == range.start);
            // When full, the positions get approximate rather than allocating on the audio thread
            if (contiguous || m_ranges.size() == capacity) {
                last.frames += range.frames;
                return;
            }
        }
        m_ranges.push_back(range);
    }

    //! Calls `visitor(range, offset)` for the oldest `frames` frames
    template<typename Visitor>
    void pop(int64_t frames, const Visitor& visitor)
    {
        int64_t offset = 0;
        while (frames > 0 && !m_ranges.empty()) {
            auto& first = m_ranges.front();
            const auto taken = std::min(frames, first.frames);
            visitor(Range { first.start, taken, first.valid }, offset);
            first.start += taken;
            first.frames -= taken;
            if (first.frames == 0) {
                m_ranges.erase(m_ranges.begin());
            }
            frames -= taken;
            offset += taken;
        }
    }

private:
    static constexpr size_t capacity = 256;
    std::vector<Range> m_ranges;
};

//! Scratch space in the layout RealtimeEffectManager::Process() expects
class EffectScratch
{
public:
    void allocate(int frames)
    {
        for (auto& buffer : m_buffers) {
            buffer.assign(frames, 0.f);
        }
        for (int i = 0; i < numChannels; ++i) {
            m_scratch[i] = m_buffers[i].data();
        }
    }

    float* const* scratch() { return m_scratch; }
    float* dummy() { return m_buffers[numChannels].data(); }

private:
    std::vector<float> m_buffers[numChannels + 1];
    float* m_scratch[numChannels] {};
};

float* channelPointer(const choc::buffer::ChannelArrayView<float>& view, int channel)
{
    return view.data.channels[channel] + view.data.offset;
}

//! Renders one playback sequence: pulls its samples from a Mixer (which resamples and stretches),
//! applies its realtime effects, mute/solo and volume/pan, and feeds its meter.
class SequenceNode final : public Node
{
public:
    //! @param graph the graph processing `sequence` as it is played, where its samples are read
    SequenceNode(std::shared_ptr<const PlayableSequence> sequence, std::shared_ptr<ISequenceProcessingNode> graph,
                 const PlayHead& playHead, const RenderContext& context, const AudacityProject& project, double mixerStartTime,
                 double mixerEndTime, int latency, size_t nodeId)
        : m_sequence{std::move(sequence)}, m_graph{std::move(graph)}, m_playHead{playHead}, m_context{context}, m_project{project},
        m_mixerStartTime{mixerStartTime}, m_mixerEndTime{mixerEndTime}, m_latency{latency}, m_nodeId{nodeId}
    {
        const auto waveTrack = dynamic_cast<const WaveTrack*>(m_sequence->FindChannelGroup());
        m_trackId = waveTrack ? static_cast<int64_t>(waveTrack->GetId()) : -1;
    }

    NodeProperties getNodeProperties() override
    {
        NodeProperties props;
        props.hasAudio = true;
        props.numberOfChannels = numChannels;
        // The summing node delays the other sequences to compensate
        props.latencyNumSamples = m_latency;
        props.nodeID = m_nodeId;
        return props;
    }

    bool isReadyToProcess() override { return true; }

    void prepareToPlay(const PlaybackInitialisationInfo& info) override
    {
        m_sampleRate = info.sampleRate;
        Mixer::Inputs inputs;
        inputs.push_back(Mixer::Input { m_sequence, {}, m_graph });
        m_mixer = std::make_unique<Mixer>(std::move(inputs), std::nullopt,
                                          // Don't throw for read errors, just play silence:
                                          false, Mixer::WarpOptions { &m_project }, m_mixerStartTime, m_mixerEndTime,
                                          m_sequence->NChannels(), info.blockSize,
                                          false, // not interleaved
                                          m_sampleRate, floatSample,
                                          false, // low quality dithering and resampling
                                          nullptr, // no custom mix-down
                                          Mixer::ApplyVolume::Discard); // volume is applied here
        for (auto& buffer : m_work) {
            buffer.assign(info.blockSize, 0.f);
        }
        m_meterBuffer.assign(info.blockSize, 0.f);
        m_scratch.allocate(info.blockSize);
        m_nextTimelinePosition.reset();
    }

    void process(ProcessContext& pc) override
    {
        const auto frames = static_cast<size_t>(pc.numSamples);
        const auto output = pc.buffers.audio;
        if (frames == 0) {
            return;
        }

        const auto sequenceChannels = std::min<size_t>(m_sequence->NChannels(), numChannels);
        // The block wraps around the loop more than once if the loop is shorter
        const auto linearStart = m_playHead.referenceSampleRangeToSourceRangeUnlooped(pc.referenceSampleRange).getStart();
        for (size_t done = 0; done < frames;) {
            const auto [start, length] = playHeadSegment(m_playHead, linearStart + static_cast<int64_t>(done),
                                                         static_cast<int64_t>(frames - done));
            if (start != m_nextTimelinePosition) {
                // A seek or a loop
                m_mixer->Reposition(start / m_sampleRate, true);
            }
            m_nextTimelinePosition = start + length;
            const auto produced = m_mixer->Process(static_cast<size_t>(length));
            for (size_t ch = 0; ch < sequenceChannels; ++ch) {
                auto* const dst = m_work[ch].data() + done;
                const auto* const src = reinterpret_cast<const float*>(m_mixer->GetBuffer(ch));
                std::copy_n(src, produced, dst);
                std::fill(dst + produced, dst + length, 0.f);
            }
            done += static_cast<size_t>(length);
        }
        for (size_t ch = sequenceChannels; ch < numChannels; ++ch) {
            // Effects may crash on null buffers, even for channels they don't use
            std::fill_n(m_work[ch].data(), frames, 0.f);
        }

        // Past the end of the play range, only what is still in the effects' latency is heard
        if (const auto end = m_context.endPosition) {
            const auto audible = static_cast<size_t>(std::clamp<int64_t>(*end - linearStart, 0, frames));
            for (auto& buffer : m_work) {
                std::fill(buffer.data() + audible, buffer.data() + frames, 0.f);
            }
        }

        if (const auto group = m_sequence->FindChannelGroup(); m_context.effects && group) {
            float* buffers[numChannels] { m_work[0].data(), m_work[1].data() };
            // Ignores how many samples to discard for the effects' latency: the output stays delayed by
            // m_latency, which the graph compensates.
            m_context.effects->Process(group, buffers, m_scratch.scratch(), m_scratch.dummy(), numChannels, frames);
        }

        const bool silenced = !m_sequence->GetSolo() && (m_context.hasSoloingSequence || m_sequence->GetMute());
        float maxVolume = 0.f;
        for (int ch = 0; ch < numChannels; ++ch) {
            const float volume = silenced ? 0.f : m_sequence->GetChannelVolume(ch);
            maxVolume = std::max(maxVolume, volume);
            const auto* const src = m_work[sequenceChannels > 1 ? ch : 0].data();
            auto* const dst = channelPointer(output, ch);
            for (size_t i = 0; i < frames; ++i) {
                dst[i] = src[i] * volume;
            }
        }

        if (const auto& meter = m_context.meter) {
            const auto trackId = au::audio::IAudioMeter::TrackId { m_trackId };
            // This output is heard after the latency it is delayed by downstream
            const auto dacTime = m_context.dacTime + toDuration(m_context.latency - m_latency, m_sampleRate);
            if (sequenceChannels > 1) {
                for (int ch = 0; ch < numChannels; ++ch) {
                    meter->push(ch, { channelPointer(output, ch), frames, 1, dacTime }, trackId);
                }
            } else {
                for (size_t i = 0; i < frames; ++i) {
                    m_meterBuffer[i] = m_work[0][i] * maxVolume;
                }
                meter->push(0, { m_meterBuffer.data(), frames, 1, dacTime }, trackId);
            }
        }
    }

private:
    const std::shared_ptr<const PlayableSequence> m_sequence;
    const std::shared_ptr<ISequenceProcessingNode> m_graph;
    const PlayHead& m_playHead;
    const RenderContext& m_context;
    const AudacityProject& m_project;
    const double m_mixerStartTime;
    const double m_mixerEndTime;
    const int m_latency;
    const size_t m_nodeId;
    int64_t m_trackId = -1;

    double m_sampleRate = 0.0;
    std::unique_ptr<Mixer> m_mixer;
    std::optional<int64_t> m_nextTimelinePosition;
    std::vector<float> m_work[numChannels];
    std::vector<float> m_meterBuffer;
    EffectScratch m_scratch;
};

//! Applies the master realtime effects to the mix of all sequences
class MasterNode final : public Node
{
public:
    MasterNode(std::unique_ptr<Node> input, const RenderContext& context, int latency, size_t nodeId)
        : m_input{std::move(input)}, m_context{context}, m_latency{latency}, m_nodeId{nodeId}
    {
    }

    NodeProperties getNodeProperties() override
    {
        NodeProperties props;
        props.hasAudio = true;
        props.numberOfChannels = numChannels;
        props.latencyNumSamples = m_input->getNodeProperties().latencyNumSamples + m_latency;
        props.nodeID = m_nodeId;
        return props;
    }

    std::vector<Node*> getDirectInputNodes() override { return { m_input.get() }; }

    bool isReadyToProcess() override { return m_input->hasProcessed(); }

    void prepareToPlay(const PlaybackInitialisationInfo& info) override
    {
        m_scratch.allocate(info.blockSize);
    }

    void process(ProcessContext& pc) override
    {
        copy(pc.buffers.audio, m_input->getProcessedOutput().audio);
        if (m_context.effects) {
            float* buffers[numChannels] { channelPointer(pc.buffers.audio, 0), channelPointer(pc.buffers.audio, 1) };
            // As for the sequences, the output stays delayed by m_latency
            m_context.effects->Process(RealtimeEffectManager::MasterGroup, buffers, m_scratch.scratch(), m_scratch.dummy(),
                                       numChannels, pc.numSamples);
        }
    }

private:
    const std::unique_ptr<Node> m_input;
    const RenderContext& m_context;
    const int m_latency;
    const size_t m_nodeId;
    EffectScratch m_scratch;
};

std::string effectiveOutputDevice(const JuceAudioDevices& devices, const au::audio::AudioConfiguration& configuration)
{
    if (configuration.outputDevice && muse::contains(devices.outputDevices(configuration.api), *configuration.outputDevice)) {
        return *configuration.outputDevice;
    }
    return devices.systemDefaultOutputDevice(configuration.api);
}

//! The requested rate if the device supports it, else its highest
double bestSampleRate(juce::AudioIODevice& device, double requested)
{
    const auto rates = device.getAvailableSampleRates();
    if (rates.isEmpty() || rates.contains(requested)) {
        return requested;
    }
    return *std::max_element(rates.begin(), rates.end());
}
}

//! A playback stream: the device, the graph it pulls, and the play head
class TracktionAudioEngine::Stream final : public juce::AudioIODeviceCallback
{
public:
    Stream(TracktionAudioEngine& engine, AudacityProject& project, int token, std::unique_ptr<juce::AudioIODevice> device,
           bool followsPlayRegion)
        : project{project}, token{token}, followsPlayRegion{followsPlayRegion}, m_engine{engine},
        m_wProject{project.shared_from_this()}, m_device{std::move(device)},
        m_sampleRate{m_device->getCurrentSampleRate()}, m_blockSize{std::max(1, m_device->getCurrentBufferSizeSamples())},
        m_buffer{numChannels, static_cast<choc::buffer::FrameCount>(m_blockSize)}
    {
        m_playHead.setReferenceSampleRange({ 0, 0 });
    }

    ~Stream() override
    {
        stop();
    }

    double sampleRate() const { return m_sampleRate; }
    int64_t toSamples(double time) const { return static_cast<int64_t>(std::llround(time * m_sampleRate)); }
    double toTime(int64_t samples) const { return samples / m_sampleRate; }

    void build(const TransportSequences& sequences, double mixerStartTime, double mixerEndTime, const audio::IAudioMeterPtr& meter)
    {
        m_sequences = sequences.playbackSequences;
        m_context.meter = meter;

        // Setup for realtime effects at the rate of the stream, not of the sequences
        m_effectsInitialization.emplace(m_wProject, m_sampleRate, numChannels, m_blockSize);
        for (const auto& sequence : m_sequences) {
            if (const auto group = sequence->FindChannelGroup()) {
                m_effectsInitialization->AddGroup(*group, numChannels, m_sampleRate, m_blockSize);
            }
        }

        // TODO: rebuild the graph when the latency changes during the stream
        size_t nodeId = 1;
        std::vector<std::unique_ptr<Node> > sequenceNodes;
        for (size_t i = 0; i < m_sequences.size(); ++i) {
            const auto& sequence = m_sequences[i];
            const auto group = sequence->FindChannelGroup();
            const auto latency = group ? effectsLatency(project, group, m_sampleRate) : 0;
            sequenceNodes.push_back(std::make_unique<SequenceNode>(sequence, sequences.playbackGraphs[i], m_playHead, m_context,
                                                                   project, mixerStartTime, mixerEndTime, latency, nodeId++));
        }
        auto master = std::make_unique<MasterNode>(std::make_unique<SummingNode>(std::move(sequenceNodes)), m_context,
                                                   effectsLatency(project, RealtimeEffectManager::MasterGroup, m_sampleRate),
                                                   nodeId++);
        auto& root = *master;
        m_player = std::make_unique<NodePlayer>(std::move(master), &m_playHeadState);
        m_player->prepareToPlay(m_sampleRate, m_blockSize);
        m_context.latency = std::max(0, root.getNodeProperties().latencyNumSamples);
        m_pendingFrames.reset();
    }

    //! Processes the graph for its latency, so that the device hears the start of the play range first.
    //! Call after setPlayRange() and before start().
    void preRoll()
    {
        for (int64_t remaining = m_context.latency; remaining > 0;) {
            const auto frames = static_cast<int>(std::min<int64_t>(remaining, m_blockSize));
            processChunk(nullptr, 0, 0, frames, Clock::now(), true);
            remaining -= frames;
        }
    }

    //! Plays from `startTime` to `endTime`, or loops `loopRange` if given, rolling into it if `startTime` is left of it.
    void setPlayRange(double startTime, double endTime, std::optional<std::pair<double, double> > loopRange)
    {
        m_endPosition = toSamples(endTime);
        const auto start = toSamples(startTime);
        if (loopRange) {
            const auto loop = toLoopRange(*loopRange);
            m_playHead.play(loop, true);
            // Unlike AudioIO, it doesn't loop ranges of 50 samples or fewer
            m_playHead.setLoopRange(true, loop, false);
            if (start < loop.getStart()) {
                m_playHead.setRollInToLoop(start);
            } else {
                m_playHead.overridePosition(start);
            }
        } else {
            m_playHead.play({ start, std::max(start, m_endPosition.load()) }, false);
        }
    }

    //! Main thread, while playing: the play region changed
    void updateLoop(std::optional<std::pair<double, double> > loopRange, double endTime)
    {
        if (loopRange) {
            m_playHead.setLoopRange(true, toLoopRange(*loopRange), true);
        } else {
            m_endPosition = toSamples(endTime);
            m_playHead.setLoopRange(false, m_playHead.getLoopRange(), true);
        }
    }

    double position() const { return toTime(m_playHead.getPosition()); }
    void seek(double time) { m_playHead.setPosition(toSamples(time)); }
    void setPaused(bool paused) { m_paused = paused; }
    bool isFinished() const { return m_finished; }

    void start()
    {
        if (m_context.meter) {
            m_context.meter->start(m_sampleRate);
        }
        m_device->start(this);
    }

    void stop()
    {
        if (m_device) {
            m_device->stop();
            m_device->close();
            m_device.reset();
        }
        if (m_context.meter) {
            m_context.meter->stop();
        }
        m_player.reset();
        // After the device stopped, so that the callback can't use them
        m_effectsInitialization.reset();
    }

    RealtimeEffects::InitializationScope* effectsInitialization()
    {
        return m_effectsInitialization ? &*m_effectsInitialization : nullptr;
    }

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputChannelData, int numOutputChannels,
                                          int numSamples, const juce::AudioIODeviceCallbackContext&) override
    {
        for (int ch = 0; ch < numOutputChannels; ++ch) {
            if (outputChannelData[ch]) {
                std::fill_n(outputChannelData[ch], numSamples, 0.f);
            }
        }

        if (m_paused || m_finished || !m_player) {
            return;
        }

        const auto latency = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(m_device->getOutputLatencyInSamples() / m_sampleRate));
        const auto dacTime = Clock::now() + latency;

        int done = 0;
        while (done < numSamples && !m_finished) {
            const int frames = std::min(numSamples - done, m_blockSize);
            const auto chunkDacTime = dacTime + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(done / m_sampleRate));
            processChunk(outputChannelData, numOutputChannels, done, frames, chunkDacTime);
            done += frames;
        }

        m_engine.m_callbackInfoQueue.Put({ dacTime, numSamples });
    }

    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    void audioDeviceError(const juce::String& errorMessage) override
    {
        LOGE() << "audio device error: " << errorMessage.toStdString();
    }

    AudacityProject& project;
    const int token;
    //! Whether loop play follows the play region, as with the default playback policy
    const bool followsPlayRegion;
    double trackEndTime = 0.0;

private:
    //! At least a sample long, to be played as a loop
    juce::Range<int64_t> toLoopRange(const std::pair<double, double>& loopRange) const
    {
        const auto start = toSamples(loopRange.first);
        return { start, std::max(start + 1, toSamples(loopRange.second)) };
    }

    //! With `preRoll`, only feeds the graph: nothing is output, metered or reported as played
    void processChunk(float* const* output, int numOutputChannels, int offset, int frames, Clock::time_point dacTime,
                      bool preRoll = false)
    {
        const juce::Range<int64_t> referenceRange { m_referencePosition, m_referencePosition + frames };
        m_playHead.setReferenceSampleRange(referenceRange);
        const auto timeline = referenceSampleRangeToSplitTimelineRange(m_playHead, referenceRange);

        // What the graph is fed now, heard after its latency. Unless looping, play stops at the end.
        if (m_playHead.isLooping()) {
            m_context.endPosition.reset();
            // The block wraps around the loop more than once if the loop is shorter
            const auto linearStart = m_playHead.referenceSampleRangeToSourceRangeUnlooped(referenceRange).getStart();
            for (int64_t done = 0; done < frames;) {
                const auto [start, length] = playHeadSegment(m_playHead, linearStart + done, frames - done);
                m_pendingFrames.push({ start, length });
                done += length;
            }
        } else {
            const auto end = m_endPosition.load();
            const auto start = timeline.timelineRange1.getStart();
            const auto inRange = std::clamp<int64_t>(end - start, 0, frames);
            m_context.endPosition = end;
            m_pendingFrames.push({ start, inRange });
            m_pendingFrames.push({ 0, frames - inRange, false });
        }

        m_context.hasSoloingSequence = std::any_of(m_sequences.begin(), m_sequences.end(),
                                                   [](const auto& sequence) { return sequence->GetSolo(); });
        m_context.dacTime = dacTime;
        std::optional<RealtimeEffects::ProcessingScope> effects;
        if (m_effectsInitialization) {
            effects.emplace(*m_effectsInitialization, m_wProject);
            m_context.effects = &*effects;
        }

        auto view = m_buffer.getView().getStart(static_cast<choc::buffer::FrameCount>(frames));
        view.clear();
        m_midi.clear();
        m_player->process({ static_cast<choc::buffer::FrameCount>(frames), referenceRange, { view, m_midi } });
        m_context.effects = nullptr;
        m_referencePosition += frames;
        if (preRoll) {
            return;
        }

        // What comes out now: report it as played, or silence it past the end
        bool finished = false;
        m_pendingFrames.pop(frames, [&](const PendingFrames::Range& range, int64_t rangeOffset) {
            if (range.valid) {
                m_engine.m_playedRanges.Put({ toTime(range.start), static_cast<unsigned long>(range.frames) });
            } else {
                view.getFrameRange({ static_cast<choc::buffer::FrameCount>(rangeOffset),
                                     static_cast<choc::buffer::FrameCount>(rangeOffset + range.frames) }).clear();
                finished = true;
            }
        });

        if (const auto& meter = m_context.meter) {
            // The meter shows the mix before the playback volume
            m_interleaved.resize(static_cast<size_t>(frames) * numChannels);
            for (int i = 0; i < frames; ++i) {
                for (int ch = 0; ch < numChannels; ++ch) {
                    m_interleaved[i * numChannels + ch] = view.getSample(ch, i);
                }
            }
            for (int ch = 0; ch < numChannels; ++ch) {
                meter->push(ch, { m_interleaved.data() + ch, static_cast<size_t>(frames), numChannels, dacTime });
            }
        }

        const float volume = m_engine.m_playbackVolume.load(std::memory_order_relaxed);
        for (int ch = 0; ch < numOutputChannels; ++ch) {
            if (!output[ch]) {
                continue;
            }
            for (int i = 0; i < frames; ++i) {
                // A mono device gets the downmix
                const float sample = numOutputChannels == 1
                                     ? 0.5f * (view.getSample(0, i) + view.getSample(1, i))
                                     : ch < numChannels ? view.getSample(ch, i) : 0.f;
                output[ch][offset + i] = sample * volume;
            }
        }

        if (finished) {
            m_finished = true;
        }
    }

    TracktionAudioEngine& m_engine;
    const std::weak_ptr<AudacityProject> m_wProject;
    std::unique_ptr<juce::AudioIODevice> m_device;
    const double m_sampleRate;
    const int m_blockSize;

    PlayHead m_playHead;
    PlayHeadState m_playHeadState { m_playHead };
    std::unique_ptr<NodePlayer> m_player;
    RenderContext m_context;
    ConstPlayableSequences m_sequences;
    std::optional<RealtimeEffects::InitializationScope> m_effectsInitialization;

    std::atomic<int64_t> m_endPosition { 0 };
    std::atomic<bool> m_paused { false };
    std::atomic<bool> m_finished { false };

    // Audio thread only, once started
    int64_t m_referencePosition = 0;
    PendingFrames m_pendingFrames;
    choc::buffer::ChannelArrayBuffer<float> m_buffer;
    tracktion_engine::MidiMessageArray m_midi;
    std::vector<float> m_interleaved;
};

TracktionAudioEngine::TracktionAudioEngine(std::shared_ptr<JuceAudioDevices> devices)
    : m_devices{std::move(devices)}
{
}

TracktionAudioEngine::~TracktionAudioEngine() = default;

void TracktionAudioEngine::setDriverController(const audio::IAudioDriverController* driverController)
{
    m_driverController = driverController;
}

bool TracktionAudioEngine::isBusy() const
{
    return m_stream != nullptr;
}

bool TracktionAudioEngine::isCapturing() const
{
    return false;
}

bool TracktionAudioEngine::isMonitoring() const
{
    return false;
}

std::optional<au::audio::AudioStreamDescriptor> TracktionAudioEngine::currentStream() const
{
    if (!m_stream) {
        return std::nullopt;
    }
    return audio::AudioStreamDescriptor { audio::AudioStreamKind::Playback, &m_stream->project, m_stream->sampleRate() };
}

bool TracktionAudioEngine::isStreamActive(AudacityProject& project) const
{
    return m_stream && &m_stream->project == &project && !m_stream->isFinished();
}

double TracktionAudioEngine::transportTime() const
{
    return m_transportTime;
}

int TracktionAudioEngine::startTransport(const TransportSequences& sequences, double startTime, double endTime,
                                         double mixerEndTime, AudacityProject& project, const StartTransportOptions& options)
{
    if (m_stream) {
        return 0;
    }
    if (!sequences.captureSequences.empty()) {
        m_lastError = u"The Tracktion audio engine cannot record yet";
        return 0;
    }
    if (sequences.playbackSequences.empty()) {
        m_lastError = u"Nothing to play";
        return 0;
    }
    // The graphs are read rather than the sequences
    IF_ASSERT_FAILED(sequences.playbackGraphs.size() == sequences.playbackSequences.size()
                     && std::all_of(sequences.playbackGraphs.begin(), sequences.playbackGraphs.end(),
                                    [](const auto& graph) { return graph != nullptr; })) {
        return 0;
    }
    IF_ASSERT_FAILED(m_driverController) {
        return 0;
    }

    const auto configuration = m_driverController->configuration();
    const auto type = m_devices->deviceType(configuration.api);
    if (!type) {
        m_lastError = muse::String::fromStdString("No such audio API: " + configuration.api);
        return 0;
    }
    const auto outputDevice = effectiveOutputDevice(*m_devices, configuration);
    std::unique_ptr<juce::AudioIODevice> device { type->createDevice(juce::String(outputDevice), {}) };
    if (!device) {
        m_lastError = muse::String::fromStdString("Cannot create audio device " + outputDevice);
        return 0;
    }

    juce::BigInteger outputChannels;
    outputChannels.setRange(0, std::min(numChannels, device->getOutputChannelNames().size()), true);
    const auto sampleRate = bestSampleRate(*device, options.sampleRate);
    const auto error = device->open({}, outputChannels, sampleRate, device->getDefaultBufferSize());
    if (error.isNotEmpty()) {
        m_lastError = muse::String::fromStdString(error.toStdString());
        return 0;
    }

    // As AudioIO: the default policy plays from the play region start, and loops it if it is active
    const auto& playRegion = ViewInfo::Get(project).playRegion;
    std::optional<double> streamStartTime = options.streamStartTime;
    if (!streamStartTime && options.isDefaultPolicy) {
        streamStartTime = std::max(0.0, playRegion.GetStart());
    }
    const double playStartTime = streamStartTime.value_or(startTime);
    const double mixerStartTime = std::min(startTime, playStartTime);

    auto stream = std::make_unique<Stream>(*this, project, m_nextStreamToken, std::move(device), options.isDefaultPolicy);
    stream->trackEndTime = TrackList::Get(project).GetEndTime();
    const auto meter = muse::value(m_playbackMeters, &project);
    // Loop play may be turned off while playing, and then plays on to the end of the tracks
    const double streamMixerEndTime = options.isDefaultPolicy ? std::max(mixerEndTime, stream->trackEndTime) : mixerEndTime;
    stream->build(sequences, mixerStartTime, streamMixerEndTime, meter);

    std::optional<std::pair<double, double> > loopRange;
    if (options.isDefaultPolicy && playRegion.Active() && startTime < endTime
        && playStartTime <= endTime + loopCaptureAllowance) {
        loopRange.emplace(startTime, endTime);
    }
    stream->setPlayRange(playStartTime, endTime, loopRange);
    stream->preRoll();

    m_callbackInfoQueue.Clear();
    m_playedRanges.Clear();
    m_currentPlayedRange = { playStartTime, 0 };
    m_consumedInCurrentRange = 0;
    m_transportTime = playStartTime;
    m_lastSampleRate = stream->sampleRate();

    m_stream = std::move(stream);
    if (m_stream->followsPlayRegion) {
        m_playRegionSubscription = ViewInfo::Get(project).playRegion.Subscribe([this](auto&) { onPlayRegionChanged(); });
    }
    m_stream->start();

    LOGI() << "Tracktion engine playing at " << m_lastSampleRate << " Hz on " << outputDevice;
    return m_nextStreamToken++;
}

void TracktionAudioEngine::onPlayRegionChanged()
{
    if (!m_stream) {
        return;
    }
    const auto& playRegion = ViewInfo::Get(m_stream->project).playRegion;
    const double t0 = playRegion.GetStart();
    const double t1 = playRegion.GetEnd();
    std::optional<std::pair<double, double> > loopRange;
    if (playRegion.Active() && t0 < t1 && m_stream->position() <= t1 + loopCaptureAllowance) {
        loopRange.emplace(t0, t1);
    }
    // Without a loop, play on to the end of the tracks
    m_stream->updateLoop(loopRange, std::max(t0, m_stream->trackEndTime));
}

void TracktionAudioEngine::stopTransport()
{
    if (!m_stream) {
        return;
    }
    m_playRegionSubscription.Reset();
    m_stream->stop();
    m_stream.reset();
    m_streamStopped.notify();
}

void TracktionAudioEngine::pauseTransport(bool pause)
{
    if (m_stream) {
        m_stream->setPaused(pause);
    }
}

void TracktionAudioEngine::seek(double time)
{
    if (m_stream) {
        m_stream->seek(time);
    }
}

std::shared_ptr<RealtimeEffectState> TracktionAudioEngine::addRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                                                  const std::string& effectId)
{
    const auto scope = m_stream && &m_stream->project == &project ? m_stream->effectsInitialization() : nullptr;
    return RealtimeEffectManager::Get(project).AddState(scope, group, au3::wxFromStdString(effectId));
}

void TracktionAudioEngine::removeRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                     const std::shared_ptr<RealtimeEffectState>& state)
{
    const auto scope = m_stream && &m_stream->project == &project ? m_stream->effectsInitialization() : nullptr;
    RealtimeEffectManager::Get(project).RemoveState(scope, group, state);
}

std::shared_ptr<RealtimeEffectState> TracktionAudioEngine::replaceRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                                                      size_t effectListIndex,
                                                                                      const std::string& newEffectId)
{
    const auto scope = m_stream && &m_stream->project == &project ? m_stream->effectsInitialization() : nullptr;
    return RealtimeEffectManager::Get(project).ReplaceState(scope, group, effectListIndex, au3::wxFromStdString(newEffectId));
}

void TracktionAudioEngine::setPlaybackMeter(AudacityProject& project, const audio::IAudioMeterPtr& meter)
{
    m_playbackMeters[&project] = meter;
}

void TracktionAudioEngine::setCaptureMeter(AudacityProject& project, const audio::IAudioMeterPtr& meter)
{
    m_captureMeters[&project] = meter;
}

void TracktionAudioEngine::stopMeters(AudacityProject& project)
{
    for (const auto* meters : { &m_playbackMeters, &m_captureMeters }) {
        if (const auto meter = muse::value(*meters, &project)) {
            meter->stop();
        }
    }
}

void TracktionAudioEngine::startMonitoring(AudacityProject&)
{
}

void TracktionAudioEngine::stopMonitoring()
{
}

void TracktionAudioEngine::setInputVolume(float newInputVolume)
{
    m_inputVolume = newInputVolume;
}

float TracktionAudioEngine::getInputVolume() const
{
    return m_inputVolume;
}

void TracktionAudioEngine::setPlaybackVolume(float newPlaybackVolume)
{
    m_playbackVolume = newPlaybackVolume;
}

float TracktionAudioEngine::getPlaybackVolume() const
{
    return m_playbackVolume;
}

bool TracktionAudioEngine::canStopTransport(AudacityProject& project) const
{
    return !m_stream || &m_stream->project == &project;
}

void TracktionAudioEngine::handleDeviceChange()
{
    // The device is opened for each stream, and the driver controller suspends streams before changing it
}

muse::String TracktionAudioEngine::lastErrorString() const
{
    return m_lastError;
}

double TracktionAudioEngine::getPlaybackSampleRate() const
{
    return m_lastSampleRate;
}

void TracktionAudioEngine::updateTimePosition(unsigned long newlyConsumedSamples)
{
    while (newlyConsumedSamples > 0) {
        if (m_consumedInCurrentRange == m_currentPlayedRange.frames) {
            PlayedRange next;
            if (!m_playedRanges.Get(next)) {
                break;
            }
            m_currentPlayedRange = next;
            m_consumedInCurrentRange = 0;
        }
        const auto consumed = std::min(newlyConsumedSamples, m_currentPlayedRange.frames - m_consumedInCurrentRange);
        m_consumedInCurrentRange += consumed;
        newlyConsumedSamples -= consumed;
    }
    if (m_lastSampleRate > 0) {
        m_transportTime = m_currentPlayedRange.time + m_consumedInCurrentRange / m_lastSampleRate;
    }
}

std::optional<au::audio::AudioCallbackInfo> TracktionAudioEngine::consumeNextCallbackInfo()
{
    audio::AudioCallbackInfo info;
    if (m_callbackInfoQueue.Get(info)) {
        return info;
    }
    return std::nullopt;
}

muse::async::Notification TracktionAudioEngine::recordingUpdateRequested() const
{
    return m_recordingUpdateRequested;
}

muse::async::Notification TracktionAudioEngine::recordingNewBlocks() const
{
    return m_recordingNewBlocks;
}

muse::async::Notification TracktionAudioEngine::recordingCommitRequested() const
{
    return m_recordingCommitRequested;
}

muse::async::Notification TracktionAudioEngine::recordingFinished() const
{
    return m_recordingFinished;
}

muse::async::Notification TracktionAudioEngine::streamStopped() const
{
    return m_streamStopped;
}

muse::async::Channel<au::au3::Au3TrackId, au::au3::Au3ClipId> TracktionAudioEngine::recordingClipChanged() const
{
    return m_recordingClipChanged;
}
