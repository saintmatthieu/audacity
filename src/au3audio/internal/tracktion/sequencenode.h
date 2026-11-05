/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "../juce/rendercontext.h"
#include "../juce/sequencerenderer.h"
#include "tracktiongraphincludes.h"

class ISequenceProcessingNode;
class LockFreeQueueNode;
class PlayableSequence;

namespace au::au3audio {
//! Renders one playback sequence, as SequenceRenderer does
/*!
 * The sequence is read, with its gain envelope, stretched and resampled to the device rate ahead of time on a worker
 * thread, where the device will read it as the play head says.
 */
class SequenceNode final : public tracktion::graph::Node
{
public:
    //! @param graph the graph processing `sequence` as it is played, where its samples are read
    SequenceNode(std::shared_ptr<const PlayableSequence> sequence, std::shared_ptr<ISequenceProcessingNode> graph,
                 const tracktion::graph::PlayHead& playHead, const RenderContext& context, int latency, size_t nodeId);
    ~SequenceNode() override;

    tracktion::graph::NodeProperties getNodeProperties() override;
    bool isReadyToProcess() override { return true; }
    void prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info) override;
    void process(tracktion::graph::Node::ProcessContext& pc) override;

    //! Starts reading the sequence ahead; call after prepareToPlay(), once the play head is set
    void startReading();
    //! Any thread, after the play head changed what it plays ahead of the device, e.g. a seek: what was read ahead
    //! isn't played, and the sequence is read again shortly ahead
    void invalidateReadAhead();
    //! After the graph processed a block, on the thread that processes it: sends what was rendered to the meter. Not
    //! while rendering, which may happen on other threads at the same time as other sequences.
    void sendToMeter() { m_renderer.sendToMeter(); }

    //! Before playing: the sequence is silent from timeline `position` on
    void stopPlayingAt(int64_t position) { m_renderer.stopPlayingAt(position); }

    //! Any thread: the reference sample that what was read ahead, or is being read, ends at
    int64_t readAheadEnd() const;
    //! At most how far ahead of the device the sequence is read
    static double maxReadAheadDuration();

private:
    const std::shared_ptr<const PlayableSequence> m_sequence;
    const std::shared_ptr<ISequenceProcessingNode> m_graph;
    const tracktion::graph::PlayHead& m_playHead;
    const int m_latency;
    const size_t m_nodeId;
    SequenceRenderer m_renderer;

    //! The sequence as read ahead on the worker thread, by reference sample
    std::shared_ptr<ISequenceProcessingNode> m_source;
    LockFreeQueueNode* m_queue = nullptr;
};
}
