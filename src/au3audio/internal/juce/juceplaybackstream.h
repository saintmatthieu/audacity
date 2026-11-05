/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "playbackstream.h"
#include "rendercontext.h"
#include "streamplayhead.h"
#include "tracknode.h"

namespace au::au4juce {
class MixNode;

//! Plays the mix of the sequences, rendered by one thread, as its play head says
/*!
 * Subclasses say where it is read ahead, by workers, and so which thread renders the mix.
 */
class JucePlaybackStream : public PlaybackStream
{
public:
    using PlaybackStream::PlaybackStream;

    void build(const TransportSequences& sequences, const audio::IAudioMeterPtr& meter, std::optional<double> leadInEndTime) final;
    //! Then starts reading ahead
    void setPlayRange(double startTime, double endTime, std::optional<std::pair<double, double> > loopRange) final;
    void updateLoop(std::optional<std::pair<double, double> > loopRange, double endTime) final;
    double position() const final { return toTime(m_playHead.position()); }
    void seek(double time) final;

protected:
    //! What is read ahead is read in chunks of this duration ...
    static constexpr double readAheadChunkDuration = 0.05;
    //! ... up to this many ahead: as AudioIO, which buffers 0.25 s in batches of 0.05 s
    static constexpr size_t readAheadNumChunks = 5;

    size_t readAheadChunkSize() const { return static_cast<size_t>(std::lround(readAheadChunkDuration * m_sampleRate)); }

    //! How the tracks read their sequences ahead, if they do
    virtual std::optional<TrackNode::ReadAhead> trackReadAhead() const = 0;
    //! Once built
    virtual void onBuilt() {}
    //! Once the play head is set
    virtual void startReadingAhead() = 0;
    //! Any thread, after the play head changed what it plays ahead of the device: what was read ahead isn't played
    virtual void invalidateReadAhead() = 0;
    //! Any thread: the stream index that what was read ahead, or is being read, ends at, as the sequences are read
    virtual int64_t readAheadEnd() const = 0;
    //! Device thread: renders the `frames` frames of the stream from `index` into `stereo`. Returns how many, from the
    //! first, it rendered: the stream advances by as many.
    virtual size_t render(float* const* stereo, int64_t index, size_t frames) = 0;

    //! Releases the tracks and the mix
    void releaseRendering() override;

    //! How far ahead of what it renders the mix reads the sequences, at most
    int64_t m_maxLatency = 0;
    std::shared_ptr<MixNode> m_mixerNode;
    //! Owned by m_mixerNode
    std::vector<TrackNode*> m_trackNodes;

private:
    int renderChunk(float* const* output, int numOutputChannels, int offset, int frames, Clock::time_point dacTime) final;

    //! At least a sample long
    StreamPlayHead::Range toLoopRange(const std::pair<double, double>& loopRange) const;
    //! What of the timeline the device plays from `from` to `end`, segment by segment
    std::vector<std::pair<int64_t, int64_t> > segments(int64_t from, int64_t end) const;

    StreamPlayHead m_playHead;
    std::atomic<int64_t> m_endPosition { 0 };

    // Device thread
    std::vector<float> m_buffer[numChannels];
    float* m_bufferPointers[numChannels] {};
};
}
