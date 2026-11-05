/*
* Audacity: A Digital Audio Editor
*/
#include "tracknode.h"

#include <algorithm>

#include "au3-clip-processing/sequence-graph/LockFreeQueueNode.h"
#include "au3-clip-processing/sequence-graph/PlaybackScheduleNode.h"
#include "au3-clip-processing/sequence-graph/ResamplingNode.h"
#include "au3-mixer/AudioIOSequences.h"

#include "streamplayhead.h"

using namespace au::au3audio;

TrackNode::TrackNode(std::shared_ptr<const PlayableSequence> sequence, std::shared_ptr<ISequenceProcessingNode> graph,
                     const StreamPlayHead& playHead, const RenderContext& context, int latency, double sampleRate, size_t maxFrames,
                     std::optional<ReadAhead> readAhead)
    // What it renders is heard where it is read: no latency for the meter to account for
    : m_playHead{playHead}, m_latency{latency}, m_maxFrames{maxFrames}, m_renderer{sequence, context, 0}
{
    const auto numSequenceChannels = sequence->NChannels();
    auto source = std::move(graph);
    if (sequence->GetRate() != sampleRate) {
        source = std::make_shared<ResamplingNode>(std::move(source), numSequenceChannels, sequence->GetRate(), sampleRate);
    }
    m_source = std::make_shared<PlaybackScheduleNode>(std::move(source), std::make_unique<StreamPlayHead::Schedule>(playHead),
                                                      numSequenceChannels);
    if (readAhead) {
        auto queue = std::make_shared<LockFreeQueueNode>(std::move(m_source), numSequenceChannels, readAhead->chunkSize,
                                                         readAhead->numChunks, readAhead->resyncLead);
        m_queue = queue.get();
        m_source = std::move(queue);
    }
    m_renderer.allocate(maxFrames);
    for (int ch = 0; ch < numChannels; ++ch) {
        m_discarded[ch].assign(maxFrames, 0.f);
        m_discardedPointers[ch] = m_discarded[ch].data();
    }
}

TrackNode::~TrackNode() = default;

void TrackNode::startReading()
{
    if (m_queue) {
        m_queue->Start();
    }
}

void TrackNode::invalidateReadAhead()
{
    if (m_queue) {
        m_queue->Invalidate();
    }
}

int64_t TrackNode::readAheadEnd() const
{
    return m_queue ? m_queue->ReadEnd().as_long_long() : 0;
}

void TrackNode::GetSamples(float* const* buffers, sampleCount start, size_t len)
{
    const auto index = start.as_long_long();
    if (m_queue) {
        // What is read from here on, including what primes the effects
        m_queue->Prepare(index);
    }
    if (m_next != index) {
        // What the effects output for the samples before `index` reaches here, delayed by their latency
        for (int64_t done = 0; done < m_latency;) {
            const auto frames = std::min<int64_t>(m_latency - done, static_cast<int64_t>(m_maxFrames));
            render(m_discardedPointers, index + done, static_cast<size_t>(frames));
            done += frames;
        }
    }
    render(buffers, index + m_latency, len);
    m_next = index + static_cast<int64_t>(len);
}

void TrackNode::render(float* const* output, int64_t index, size_t len)
{
    m_source->GetSamples(m_renderer.input(), index, len);
    m_renderer.render(output, len, m_playHead.linearPosition(index));
}
