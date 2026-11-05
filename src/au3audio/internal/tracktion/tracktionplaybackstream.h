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

#include "../juce/playbackstream.h"
#include "pendingframes.h"
#include "tracktiongraphincludes.h"

namespace au::au3audio {
class SequenceNode;

//! Plays a tracktion_graph, processed on a pool of threads in step with the device, as the play head says
class TracktionPlaybackStream final : public PlaybackStream
{
public:
    using PlaybackStream::PlaybackStream;
    ~TracktionPlaybackStream() override;

    void build(const TransportSequences& sequences, const audio::IAudioMeterPtr& meter, std::optional<double> leadInEndTime) override;
    void setPlayRange(double startTime, double endTime, std::optional<std::pair<double, double> > loopRange) override;
    //! Processes the graph for its latency
    void preRoll() override;
    void updateLoop(std::optional<std::pair<double, double> > loopRange, double endTime) override;
    double position() const override { return toTime(m_playHead.getPosition()); }
    void seek(double time) override;

private:
    const char* name() const override { return "Tracktion"; }
    void renderChunk(float* const* output, int numOutputChannels, int offset, int frames, Clock::time_point dacTime) override;
    void releaseRendering() override;

    //! At least a sample long, to be played as a loop
    juce::Range<int64_t> toLoopRange(const std::pair<double, double>& loopRange) const;
    //! What of the timeline the device plays from `from` to `end`, segment by segment
    std::vector<std::pair<int64_t, int64_t> > segments(int64_t from, int64_t end) const;
    void invalidateReadAhead();

    //! With `preRoll`, only feeds the graph: nothing is output, metered or reported as played
    void processChunk(float* const* output, int numOutputChannels, int offset, int frames, Clock::time_point dacTime, bool preRoll = false);

    tracktion::graph::PlayHead m_playHead;
    //! Processes the sequences on a pool of threads, in step with the device
    std::unique_ptr<tracktion::graph::LockFreeMultiThreadedNodePlayer> m_player;
    //! Owned by m_player
    std::vector<SequenceNode*> m_sequenceNodes;
    RenderContext m_context;

    std::atomic<int64_t> m_endPosition { 0 };

    //! The reference sample the device is at; written by the audio thread once started, read by the main thread
    std::atomic<int64_t> m_referencePosition { 0 };

    // Audio thread only, once started
    PendingFrames m_pendingFrames;
    choc::buffer::ChannelArrayBuffer<float> m_buffer { numChannels, static_cast<choc::buffer::FrameCount>(m_blockSize) };
    tracktion_engine::MidiMessageArray m_midi;
};
}
