/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "effectscratch.h"
#include "rendercontext.h"

class PlayableSequence;

namespace au::au3audio {
//! Renders one playback sequence: applies its realtime effects, mute/solo and volume/pan, and keeps what its meter shows
class SequenceRenderer
{
public:
    //! @param latency of the sequence's effects, which delay what it renders
    SequenceRenderer(std::shared_ptr<const PlayableSequence> sequence, const RenderContext& context, int latency);

    const PlayableSequence& sequence() const { return *m_sequence; }

    //! Before rendering: renders at most `maxFrames` at once
    void allocate(size_t maxFrames);
    //! Before playing: the sequence is silent from timeline `position` on
    void stopPlayingAt(int64_t position) { m_stopPosition = position; }

    //! Where the caller reads the samples of the sequence to render, one buffer per channel of the device
    float* const* input() { return m_input; }

    //! Renders the `frames` frames of input(), read from `linearStart`, the linear timeline position of the first, into
    //! `output`, one buffer per channel of the device. The context's effects must be set.
    void render(float* const* output, size_t frames, int64_t linearStart);

    //! Sends what was rendered last to the meter. Not while rendering, which may happen on other threads at the same time
    //! as other sequences.
    void sendToMeter();

private:
    const std::shared_ptr<const PlayableSequence> m_sequence;
    const RenderContext& m_context;
    const int m_latency;
    int64_t m_trackId = -1;
    std::optional<int64_t> m_stopPosition;

    std::vector<float> m_work[numChannels];
    float* m_input[numChannels] {};
    //! What the meter shows of the last block rendered, one buffer per channel
    std::vector<float> m_meterBuffers[numChannels];
    size_t m_meterChannels = 0;
    size_t m_meterFrames = 0;
    audio::TimePoint m_meterTime;
    EffectScratch m_scratch;
};

//! Applies the master's realtime effects to `stereo`. The context's effects must be set.
void renderMaster(const RenderContext& context, EffectScratch& scratch, float* const* stereo, size_t frames);
}
