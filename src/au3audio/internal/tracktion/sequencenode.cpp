/*
* Audacity: A Digital Audio Editor
*/
#include "sequencenode.h"

#include <algorithm>
#include <cmath>

#include "au3-clip-processing/sequence-graph/LockFreeQueueNode.h"
#include "au3-clip-processing/sequence-graph/PlaybackScheduleNode.h"
#include "au3-clip-processing/sequence-graph/ResamplingNode.h"
#include "au3-mixer/AudioIOSequences.h"
#include "au3-realtime-effects/RealtimeEffectManager.h"
#include "au3-wave-track/WaveTrack.h"

#include "playheadschedule.h"

using namespace au::au3audio;

namespace {
//! The worker reads ahead in chunks of this duration ...
constexpr double readAheadChunkDuration = 0.05;
//! ... up to this many: as AudioIO, which buffers 0.25 s in batches of 0.05 s
constexpr size_t readAheadNumChunks = 5;
//! If the worker can't keep up, it resumes this many chunks ahead of the device
constexpr size_t resyncLeadNumChunks = 2;
}

SequenceNode::SequenceNode(std::shared_ptr<const PlayableSequence> sequence, std::shared_ptr<ISequenceProcessingNode> graph,
                           const tracktion::graph::PlayHead& playHead, const RenderContext& context, int latency,
                           size_t nodeId)
    : m_sequence{std::move(sequence)}, m_graph{std::move(graph)}, m_playHead{playHead}, m_context{context}, m_latency{latency},
    m_nodeId{nodeId}
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

    const auto numSequenceChannels = m_sequence->NChannels();
    auto source = m_graph;
    if (m_sequence->GetRate() != m_sampleRate) {
        source = std::make_unique<ResamplingNode>(std::move(source), numSequenceChannels, m_sequence->GetRate(), m_sampleRate);
    }
    const auto chunkSize = static_cast<size_t>(std::lround(readAheadChunkDuration * m_sampleRate));
    const auto resyncLead = resyncLeadNumChunks * chunkSize;
    // The device plays the timeline as the play head says, read ahead
    auto scheduled = std::make_shared<PlaybackScheduleNode>(std::move(source), std::make_unique<PlayHeadSchedule>(m_playHead),
                                                            numSequenceChannels);
    auto queue = std::make_unique<LockFreeQueueNode>(std::move(scheduled), numSequenceChannels, chunkSize, readAheadNumChunks,
                                                     resyncLead);
    m_queue = queue.get();
    m_source = std::move(queue);

    for (auto& buffer : m_work) {
        buffer.assign(info.blockSize, 0.f);
    }
    m_meterBuffer.assign(info.blockSize, 0.f);
    m_scratch.allocate(info.blockSize);
}

void SequenceNode::startReading()
{
    m_queue->Start();
}

void SequenceNode::invalidateReadAhead()
{
    m_queue->Invalidate();
}

int64_t SequenceNode::readAheadEnd() const
{
    return m_queue->ReadEnd().as_long_long();
}

double SequenceNode::maxReadAheadDuration()
{
    // After resynchronizing, with a chunk being read
    return (resyncLeadNumChunks + readAheadNumChunks + 1) * readAheadChunkDuration;
}

void SequenceNode::process(tracktion::graph::Node::ProcessContext& pc)
{
    const auto frames = static_cast<size_t>(pc.numSamples);
    const auto output = pc.buffers.audio;
    if (frames == 0) {
        return;
    }

    const auto sequenceChannels = std::min<size_t>(m_sequence->NChannels(), numChannels);
    float* sequenceBuffers[numChannels] { m_work[0].data(), m_work[1].data() };
    const auto referenceStart = pc.referenceSampleRange.getStart();
    m_queue->Prepare(referenceStart);
    m_source->GetSamples(sequenceBuffers, referenceStart, frames);
    for (size_t ch = sequenceChannels; ch < numChannels; ++ch) {
        // Effects may crash on null buffers, even for channels they don't use
        std::fill_n(m_work[ch].data(), frames, 0.f);
    }

    // Past the end of the play range, only what is still in the effects' latency is heard
    if (const auto end = m_context.endPosition) {
        const auto linearStart = m_playHead.referenceSamplePositionToTimelinePositionUnlooped(referenceStart);
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
