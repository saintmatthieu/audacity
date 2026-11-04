/*
* Audacity: A Digital Audio Editor
*/
#include "playbackstream.h"

#include <algorithm>
#include <chrono>

#include "framework/global/log.h"

#include "au3-project/Project.h"
#include "au3-track/Track.h"

#include "effectslatency.h"
#include "masternode.h"
#include "playheadsegment.h"
#include "sequencenode.h"

using namespace au::au3audio;

namespace {
using Segments = std::vector<std::pair<int64_t, int64_t> >;

//! The segments that start at `from`, cut at `end`
Segments cut(const Segments& segments, int64_t from, int64_t end)
{
    Segments result;
    for (const auto& [position, length] : segments) {
        if (from >= end) {
            break;
        }
        result.push_back({ position, std::min(length, end - from) });
        from += length;
    }
    return result;
}
}

PlaybackStream::PlaybackStream(AudacityProject& project, int token, std::unique_ptr<juce::AudioIODevice> device,
                               bool followsPlayRegion, LockFreeQueue<audio::AudioCallbackInfo>& callbackInfos,
                               LockFreeQueue<PlayedRange>& playedRanges, const std::atomic<float>& playbackVolume)
    : project{project}, token{token}, followsPlayRegion{followsPlayRegion}, m_callbackInfos{callbackInfos},
    m_playedRanges{playedRanges}, m_playbackVolume{playbackVolume}, m_wProject{project.shared_from_this()},
    m_device{std::move(device)}, m_sampleRate{m_device->getCurrentSampleRate()},
    m_blockSize{std::max(1, m_device->getCurrentBufferSizeSamples())},
    m_buffer{numChannels, static_cast<choc::buffer::FrameCount>(m_blockSize)}
{
    m_playHead.setReferenceSampleRange({ 0, 0 });
}

PlaybackStream::~PlaybackStream()
{
    stop();
}

void PlaybackStream::build(const TransportSequences& sequences, const audio::IAudioMeterPtr& meter)
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
    std::vector<std::unique_ptr<tracktion::graph::Node> > sequenceNodes;
    for (size_t i = 0; i < m_sequences.size(); ++i) {
        const auto& sequence = m_sequences[i];
        const auto group = sequence->FindChannelGroup();
        const auto latency = group ? effectsLatency(project, group, m_sampleRate) : 0;
        auto node = std::make_unique<SequenceNode>(sequence, sequences.playbackGraphs[i], m_playHead, m_context, latency,
                                                   nodeId++);
        m_sequenceNodes.push_back(node.get());
        sequenceNodes.push_back(std::move(node));
    }
    auto master = std::make_unique<MasterNode>(std::make_unique<tracktion::graph::SummingNode>(std::move(sequenceNodes)), m_context,
                                               effectsLatency(project, RealtimeEffectManager::MasterGroup, m_sampleRate),
                                               nodeId++);
    auto& root = *master;
    m_player = std::make_unique<tracktion::graph::NodePlayer>(std::move(master), &m_playHeadState);
    m_player->prepareToPlay(m_sampleRate, m_blockSize);
    m_context.latency = std::max(0, root.getNodeProperties().latencyNumSamples);
    m_pendingFrames.reset();
}

void PlaybackStream::preRoll()
{
    for (int64_t remaining = m_context.latency; remaining > 0;) {
        const auto frames = static_cast<int>(std::min<int64_t>(remaining, m_blockSize));
        processChunk(nullptr, 0, 0, frames, Clock::now(), true);
        remaining -= frames;
    }
}

void PlaybackStream::setPlayRange(double startTime, double endTime, std::optional<std::pair<double, double> > loopRange)
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
    for (const auto node : m_sequenceNodes) {
        node->startReading();
    }
}

void PlaybackStream::seek(double time)
{
    m_playHead.setPosition(toSamples(time));
    invalidateReadAhead();
}

void PlaybackStream::updateLoop(std::optional<std::pair<double, double> > loopRange, double endTime)
{
    // From where the device is, further than the sequences may have been read ahead
    const auto from = m_referencePosition.load(std::memory_order_relaxed);
    const auto horizon = from + toSamples(SequenceNode::maxReadAheadDuration());
    const auto before = segments(from, horizon);
    if (loopRange) {
        m_playHead.setLoopRange(true, toLoopRange(*loopRange), true);
    } else {
        m_endPosition = toSamples(endTime);
        m_playHead.setLoopRange(false, m_playHead.getLoopRange(), true);
    }
    // Only if what was read ahead changes: e.g. not when the end of the loop moves while still far ahead. Loaded after
    // the change, what was read before it is included.
    int64_t readEnd = 0;
    for (const auto node : m_sequenceNodes) {
        readEnd = std::max(readEnd, node->readAheadEnd());
    }
    const auto end = std::min(readEnd, horizon);
    if (cut(segments(from, horizon), from, end) != cut(before, from, end)) {
        invalidateReadAhead();
    }
}

std::vector<std::pair<int64_t, int64_t> > PlaybackStream::segments(int64_t from, int64_t end) const
{
    std::vector<std::pair<int64_t, int64_t> > segments;
    for (auto index = from; index < end;) {
        const auto linearPosition = m_playHead.referenceSamplePositionToTimelinePositionUnlooped(index);
        segments.push_back(playHeadSegment(m_playHead, linearPosition, end - index));
        index += segments.back().second;
    }
    return segments;
}

void PlaybackStream::invalidateReadAhead()
{
    // After the play head changed, so that the sequences are read again as it now says
    for (const auto node : m_sequenceNodes) {
        node->invalidateReadAhead();
    }
}

juce::Range<int64_t> PlaybackStream::toLoopRange(const std::pair<double, double>& loopRange) const
{
    const auto start = toSamples(loopRange.first);
    return { start, std::max(start + 1, toSamples(loopRange.second)) };
}

void PlaybackStream::start()
{
    if (m_context.meter) {
        m_context.meter->start(m_sampleRate);
    }
    m_device->start(this);
}

void PlaybackStream::stop()
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
    m_sequenceNodes.clear();
    // After the device stopped, so that the callback can't use them
    m_effectsInitialization.reset();
}

RealtimeEffects::InitializationScope* PlaybackStream::effectsInitialization()
{
    return m_effectsInitialization ? &*m_effectsInitialization : nullptr;
}

void PlaybackStream::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputChannelData,
                                                      int numOutputChannels, int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
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

    m_callbackInfos.Put({ dacTime, numSamples });
}

void PlaybackStream::audioDeviceError(const juce::String& errorMessage)
{
    LOGE() << "audio device error: " << errorMessage.toStdString();
}

void PlaybackStream::processChunk(float* const* output, int numOutputChannels, int offset, int frames, Clock::time_point dacTime,
                                  bool preRoll)
{
    const auto referencePosition = m_referencePosition.load(std::memory_order_relaxed);
    const juce::Range<int64_t> referenceRange { referencePosition, referencePosition + frames };
    m_playHead.setReferenceSampleRange(referenceRange);
    const auto timeline = tracktion::graph::referenceSampleRangeToSplitTimelineRange(m_playHead, referenceRange);

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
    m_referencePosition.store(referencePosition + frames, std::memory_order_relaxed);
    if (preRoll) {
        return;
    }

    // What comes out now: report it as played, or silence it past the end
    bool finished = false;
    m_pendingFrames.pop(frames, [&](const PendingFrames::Range& range, int64_t rangeOffset) {
        if (range.valid) {
            m_playedRanges.Put({ toTime(range.start), static_cast<unsigned long>(range.frames) });
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

    const float volume = m_playbackVolume.load(std::memory_order_relaxed);
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
