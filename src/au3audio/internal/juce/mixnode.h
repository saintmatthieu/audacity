/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "au3-mixer/AudioIOSequences.h"
#include "au3-mixer/ISequenceProcessingNode.h"
#include "au3-realtime-effects/RealtimeEffectManager.h"

#include "effectscratch.h"
#include "rendercontext.h"

class AudacityProject;

namespace au::au4juce {
class StreamPlayHead;
class TrackNode;

//! The mix of the playback sequences, with the master's realtime effects, as the stream plays it
/*!
 * Read by stream index, in stereo, by one thread at a time, which also sends to the meters. As the tracks do, it reads
 * them ahead by the master effects' latency, and primes the master effects wherever reading jumps.
 */
class MixNode final : public ISequenceProcessingNode
{
public:
    struct Settings {
        std::weak_ptr<AudacityProject> project;
        ConstPlayableSequences sequences;
        int masterLatency = 0;
        double sampleRate = 0.0;
        //! Renders at most this many frames at once
        size_t maxFrames = 0;
        //! How long after the device is at an index it is heard
        Clock::duration outputLatency {};
    };

    //! The context is written while rendering, which it is for; `effects`, `playHead` and `endPosition` must outlive this
    MixNode(std::vector<std::unique_ptr<TrackNode> > tracks, Settings settings, RenderContext& context,
            RealtimeEffects::InitializationScope& effects, const StreamPlayHead& playHead, const std::atomic<int64_t>& endPosition);
    ~MixNode() override;

    void GetSamples(float* const* buffers, sampleCount start, size_t len) override;

private:
    //! Renders `frames` frames from `index` on, at most maxFrames
    void render(float* const* output, int64_t index, size_t frames);
    //! The tracks' mix from `index` on, before the master effects
    void mix(float* const* output, int64_t index, size_t frames);
    void sendToMeters(float* const* output, size_t frames);

    const std::vector<std::unique_ptr<TrackNode> > m_tracks;
    const Settings m_settings;
    RenderContext& m_context;
    RealtimeEffects::InitializationScope& m_effects;
    const StreamPlayHead& m_playHead;
    const std::atomic<int64_t>& m_endPosition;

    std::optional<int64_t> m_next;
    EffectScratch m_scratch;
    std::vector<float> m_track[numChannels];
    float* m_trackPointers[numChannels] {};
    std::vector<float> m_discarded[numChannels];
    float* m_discardedPointers[numChannels] {};
    std::vector<float> m_interleaved;
};
}
