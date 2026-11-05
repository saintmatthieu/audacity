/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "au3-clip-processing/sequence-graph/LockFreeQueueNode.h"

#include "playbackstream.h"
#include "rendercontext.h"
#include "streamplayhead.h"

namespace au::au3audio {
class MixNode;

//! Plays the mix of the sequences, rendered ahead on a worker thread, as AudioIO does
/*!
 * The worker renders everything, effects included, by one thread, about 0.25 s ahead of the device, into a queue: the
 * device only copies from it. Edits are heard after what is queued; a seek or a change of what loops is heard shortly,
 * after silence while the worker renders it.
 *
 * As AudioIO, the stream advances by what the device plays: if the worker is late, the device gets silence, and then
 * what was late, rather than skipping it.
 */
class JuceWorkerPlaybackStream final : public PlaybackStream
{
public:
    using PlaybackStream::PlaybackStream;
    ~JuceWorkerPlaybackStream() override;

    void build(const TransportSequences& sequences, const audio::IAudioMeterPtr& meter, std::optional<double> leadInEndTime) override;
    //! Then starts rendering ahead
    void setPlayRange(double startTime, double endTime, std::optional<std::pair<double, double> > loopRange) override;
    void updateLoop(std::optional<std::pair<double, double> > loopRange, double endTime) override;
    double position() const override { return toTime(m_playHead.position()); }
    void seek(double time) override;

private:
    const char* name() const override { return "JUCE"; }
    std::string dropoutDetails() const override;
    int renderChunk(float* const* output, int numOutputChannels, int offset, int frames, Clock::time_point dacTime) override;
    void releaseRendering() override;

    //! At least a sample long
    StreamPlayHead::Range toLoopRange(const std::pair<double, double>& loopRange) const;
    //! What of the timeline the device plays from `from` to `end`, segment by segment
    std::vector<std::pair<int64_t, int64_t> > segments(int64_t from, int64_t end) const;

    StreamPlayHead m_playHead;
    //! Written by the worker only, once rendering
    RenderContext m_context;
    std::atomic<int64_t> m_endPosition { 0 };
    //! How far ahead of what it renders the mix reads the sequences, at most
    int64_t m_maxLatency = 0;
    std::shared_ptr<MixNode> m_mix;
    std::unique_ptr<LockFreeQueueNode> m_queue;

    // Device thread
    //! Frames the device got silence for, waiting for the worker
    int64_t m_unplayedFrames = 0;
    std::vector<float> m_buffer[numChannels];
    float* m_bufferPointers[numChannels] {};
};
}
