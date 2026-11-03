/*
* Audacity: A Digital Audio Editor
*/
#include "sequencenode.h"

#include <algorithm>

#include "au3-mixer/AudioIOSequences.h"
#include "au3-mixer/Mix.h"
#include "au3-realtime-effects/RealtimeEffectManager.h"
#include "au3-wave-track/WaveTrack.h"

#include "playheadsegment.h"

using namespace au::au3audio;

SequenceNode::SequenceNode(std::shared_ptr<const PlayableSequence> sequence, std::shared_ptr<ISequenceProcessingNode> graph,
                           const tracktion::graph::PlayHead& playHead, const RenderContext& context,
                           const AudacityProject& project, double mixerStartTime, double mixerEndTime, int latency,
                           size_t nodeId)
    : m_sequence{std::move(sequence)}, m_graph{std::move(graph)}, m_playHead{playHead}, m_context{context}, m_project{project},
    m_mixerStartTime{mixerStartTime}, m_mixerEndTime{mixerEndTime}, m_latency{latency}, m_nodeId{nodeId}
{
    const auto waveTrack = dynamic_cast<const WaveTrack*>(m_sequence->FindChannelGroup());
    m_trackId = waveTrack ? static_cast<int64_t>(waveTrack->GetId()) : -1;
}

SequenceNode::~SequenceNode() = default;

tracktion::graph::NodeProperties SequenceNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = true;
    props.numberOfChannels = numChannels;
    // The summing node delays the other sequences to compensate
    props.latencyNumSamples = m_latency;
    props.nodeID = m_nodeId;
    return props;
}

void SequenceNode::prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info)
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

void SequenceNode::process(tracktion::graph::Node::ProcessContext& pc)
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
        const auto trackId = audio::IAudioMeter::TrackId { m_trackId };
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
