/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "au3-clip-processing/sequence-graph/LockFreeQueueNode.h"

#include "juceplaybackstream.h"

namespace au::au4juce {
//! Plays the mix of the sequences, rendered ahead on a worker thread, as AudioIO does
/*!
 * The worker renders everything, effects included, by one thread, about 0.25 s ahead of the device, into a queue: the
 * device only copies from it. Edits are heard after what is queued; a seek or a change of what loops is heard shortly,
 * after silence while the worker renders it.
 *
 * As AudioIO, the stream advances by what the device plays: if the worker is late, the device gets silence, and then
 * what was late, rather than skipping it.
 */
class JuceWorkerPlaybackStream final : public JucePlaybackStream
{
public:
    using JucePlaybackStream::JucePlaybackStream;
    ~JuceWorkerPlaybackStream() override;

private:
    const char* name() const override { return "JUCE"; }
    std::string dropoutDetails() const override;
    void releaseRendering() override;

    std::optional<TrackNode::ReadAhead> trackReadAhead() const override { return std::nullopt; }
    void onBuilt() override;
    void startReadingAhead() override;
    void invalidateReadAhead() override;
    int64_t readAheadEnd() const override;
    size_t render(float* const* stereo, int64_t index, size_t frames) override;

    std::unique_ptr<LockFreeQueueNode> m_queue;
    //! Device thread: frames the device got silence for, waiting for the worker
    int64_t m_unplayedFrames = 0;
};
}
