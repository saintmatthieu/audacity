/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "au3-mixer/ISequenceProcessingNode.h"

#include "rendercontext.h"
#include "sequencerenderer.h"

class LockFreeQueueNode;
class PlayableSequence;

namespace au::au4juce {
class StreamPlayHead;

//! One playback sequence as the stream plays it, rendered as SequenceRenderer does, compensating its effects' latency
/*!
 * Read by stream index, in stereo, at most `maxFrames` at once. What is read at an index is heard there: as AudioIO
 * does, the sequence is read ahead by its effects' latency, and wherever reading jumps, the effects are first fed for
 * as long, and what they output then is dropped.
 *
 * Optionally, as the Tracktion backend's sequences, the sequence is read ahead on a worker thread, as the play head
 * says, and its effects are rendered by the reader.
 */
class TrackNode final : public ISequenceProcessingNode
{
public:
    //! How the sequence is read ahead: see LockFreeQueueNode
    struct ReadAhead {
        size_t chunkSize = 0;
        size_t numChunks = 0;
        size_t resyncLead = 0;
    };

    //! @param graph the graph processing `sequence` as it is played, where its samples are read
    //! @param latency of the sequence's effects
    TrackNode(std::shared_ptr<const PlayableSequence> sequence, std::shared_ptr<ISequenceProcessingNode> graph,
              const StreamPlayHead& playHead, const RenderContext& context, int latency, double sampleRate, size_t maxFrames,
              std::optional<ReadAhead> readAhead = std::nullopt);
    ~TrackNode() override;

    // If reading ahead
    //! Starts reading ahead, once the play head is set
    void startReading();
    //! Any thread, after the play head changed what it plays ahead of the device: what was read ahead isn't played,
    //! and the sequence is read again shortly ahead
    void invalidateReadAhead();
    //! Any thread: the stream index that what was read ahead, or is being read, ends at
    int64_t readAheadEnd() const;

    //! Before playing: the sequence is silent from timeline `position` on
    void stopPlayingAt(int64_t position) { m_renderer.stopPlayingAt(position); }

    //! The context's effects must be set
    void GetSamples(float* const* buffers, sampleCount start, size_t len) override;

    //! Sends what was rendered last to the meter
    void sendToMeter() { m_renderer.sendToMeter(); }

private:
    //! Renders what the stream plays from `index` on, as delayed by the effects
    void render(float* const* output, int64_t index, size_t len);

    const StreamPlayHead& m_playHead;
    const int m_latency;
    const size_t m_maxFrames;
    SequenceRenderer m_renderer;
    //! The sequence as the stream plays it, by stream index
    std::shared_ptr<ISequenceProcessingNode> m_source;
    //! m_source if reading ahead
    LockFreeQueueNode* m_queue = nullptr;
    //! Where reading continues without jumping
    std::optional<int64_t> m_next;
    std::vector<float> m_discarded[numChannels];
    float* m_discardedPointers[numChannels] {};
};
}
