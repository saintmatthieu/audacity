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

#include "audio/iaudioengine.h"

#include "au3-mixer/AudioIOSequences.h"
#include "au3-realtime-effects/RealtimeEffectManager.h"
#include "au3-utility/LockFreeQueue.h"

#include "pendingframes.h"
#include "playedrange.h"
#include "rendercontext.h"
#include "tracktiongraphincludes.h"

class AudacityProject;

namespace au::au3audio {
//! A playback stream: the device, the graph it pulls, and the play head
class PlaybackStream final : public juce::AudioIODeviceCallback
{
public:
    //! The stream reports what it plays to `callbackInfos` and `playedRanges`, and applies `playbackVolume`
    PlaybackStream(AudacityProject& project, int token, std::unique_ptr<juce::AudioIODevice> device, bool followsPlayRegion,
                   LockFreeQueue<audio::AudioCallbackInfo>& callbackInfos, LockFreeQueue<PlayedRange>& playedRanges,
                   const std::atomic<float>& playbackVolume);
    ~PlaybackStream() override;

    double sampleRate() const { return m_sampleRate; }
    int64_t toSamples(double time) const { return static_cast<int64_t>(std::llround(time * m_sampleRate)); }
    double toTime(int64_t samples) const { return samples / m_sampleRate; }

    void build(const TransportSequences& sequences, double mixerStartTime, double mixerEndTime, const audio::IAudioMeterPtr& meter);

    //! Processes the graph for its latency, so that the device hears the start of the play range first.
    //! Call after setPlayRange() and before start().
    void preRoll();

    //! Plays from `startTime` to `endTime`, or loops `loopRange` if given, rolling into it if `startTime` is left of it.
    void setPlayRange(double startTime, double endTime, std::optional<std::pair<double, double> > loopRange);

    //! Main thread, while playing: the play region changed
    void updateLoop(std::optional<std::pair<double, double> > loopRange, double endTime);

    double position() const { return toTime(m_playHead.getPosition()); }
    void seek(double time) { m_playHead.setPosition(toSamples(time)); }
    void setPaused(bool paused) { m_paused = paused; }
    bool isFinished() const { return m_finished; }

    void start();
    void stop();

    RealtimeEffects::InitializationScope* effectsInitialization();

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputChannelData, int numOutputChannels, int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
    void audioDeviceError(const juce::String& errorMessage) override;

    AudacityProject& project;
    const int token;
    //! Whether loop play follows the play region, as with the default playback policy
    const bool followsPlayRegion;
    double trackEndTime = 0.0;

private:
    //! At least a sample long, to be played as a loop
    juce::Range<int64_t> toLoopRange(const std::pair<double, double>& loopRange) const;

    //! With `preRoll`, only feeds the graph: nothing is output, metered or reported as played
    void processChunk(float* const* output, int numOutputChannels, int offset, int frames, Clock::time_point dacTime, bool preRoll = false);

    LockFreeQueue<audio::AudioCallbackInfo>& m_callbackInfos;
    LockFreeQueue<PlayedRange>& m_playedRanges;
    const std::atomic<float>& m_playbackVolume;

    const std::weak_ptr<AudacityProject> m_wProject;
    std::unique_ptr<juce::AudioIODevice> m_device;
    const double m_sampleRate;
    const int m_blockSize;

    tracktion::graph::PlayHead m_playHead;
    tracktion::graph::PlayHeadState m_playHeadState { m_playHead };
    std::unique_ptr<tracktion::graph::NodePlayer> m_player;
    RenderContext m_context;
    ConstPlayableSequences m_sequences;
    std::optional<RealtimeEffects::InitializationScope> m_effectsInitialization;

    std::atomic<int64_t> m_endPosition { 0 };
    std::atomic<bool> m_paused { false };
    std::atomic<bool> m_finished { false };

    // Audio thread only, once started
    int64_t m_referencePosition = 0;
    PendingFrames m_pendingFrames;
    choc::buffer::ChannelArrayBuffer<float> m_buffer;
    tracktion_engine::MidiMessageArray m_midi;
    std::vector<float> m_interleaved;
};
}
