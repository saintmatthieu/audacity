/*
* Audacity: A Digital Audio Editor
*/
#include "sequencenode.h"

#include <cmath>

#include "au3-clip-processing/sequence-graph/LockFreeQueueNode.h"
#include "au3-clip-processing/sequence-graph/PlaybackScheduleNode.h"
#include "au3-clip-processing/sequence-graph/ResamplingNode.h"
#include "au3-mixer/AudioIOSequences.h"

#include "playheadschedule.h"

using namespace au::au4tracktion;

namespace {
//! The worker reads ahead in chunks of this duration ...
constexpr double readAheadChunkDuration = 0.05;
//! ... up to this many: as AudioIO, which buffers 0.25 s in batches of 0.05 s
constexpr size_t readAheadNumChunks = 5;
//! If the worker can't keep up, it resumes this many chunks ahead of the device
constexpr size_t resyncLeadNumChunks = 2;
}

SequenceNode::SequenceNode(std::shared_ptr<const PlayableSequence> sequence, std::shared_ptr<ISequenceProcessingNode> graph,
                           const tracktion::graph::PlayHead& playHead, const au4juce::RenderContext& context, int latency,
                           size_t nodeId)
    : m_sequence{std::move(sequence)}, m_graph{std::move(graph)}, m_playHead{playHead}, m_latency{latency}, m_nodeId{nodeId},
    m_renderer{m_sequence, context, latency}
{
}

SequenceNode::~SequenceNode() = default;

tracktion::graph::NodeProperties SequenceNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = true;
    props.numberOfChannels = au4juce::numChannels;
    // The summing node delays the other sequences to compensate
    props.latencyNumSamples = m_latency;
    props.nodeID = m_nodeId;
    return props;
}

void SequenceNode::prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info)
{
    const auto sampleRate = info.sampleRate;
    const auto numSequenceChannels = m_sequence->NChannels();
    auto source = m_graph;
    if (m_sequence->GetRate() != sampleRate) {
        source = std::make_unique<ResamplingNode>(std::move(source), numSequenceChannels, m_sequence->GetRate(), sampleRate);
    }
    const auto chunkSize = static_cast<size_t>(std::lround(readAheadChunkDuration * sampleRate));
    const auto resyncLead = resyncLeadNumChunks * chunkSize;
    // The device plays the timeline as the play head says, read ahead
    auto scheduled = std::make_shared<PlaybackScheduleNode>(std::move(source), std::make_unique<PlayHeadSchedule>(m_playHead),
                                                            numSequenceChannels);
    auto queue = std::make_unique<LockFreeQueueNode>(std::move(scheduled), numSequenceChannels, chunkSize, readAheadNumChunks,
                                                     resyncLead);
    m_queue = queue.get();
    m_source = std::move(queue);

    m_renderer.allocate(static_cast<size_t>(info.blockSize));
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
    if (frames == 0) {
        return;
    }
    const auto referenceStart = pc.referenceSampleRange.getStart();
    m_queue->Prepare(referenceStart);
    m_source->GetSamples(m_renderer.input(), referenceStart, frames);
    float* output[au4juce::numChannels] { channelPointer(pc.buffers.audio, 0), channelPointer(pc.buffers.audio, 1) };
    m_renderer.render(output, frames, m_playHead.referenceSamplePositionToTimelinePositionUnlooped(referenceStart));
}
