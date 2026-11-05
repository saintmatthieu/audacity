/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "audio/iaudioengine.h"

#include "au3-mixer/AudioIOSequences.h"
#include "au3-realtime-effects/RealtimeEffectManager.h"
#include "au3-utility/LockFreeQueue.h"

#include "juceincludes.h"
#include "playedrange.h"
#include "rendercontext.h"

class AudacityProject;

namespace au::au3audio {
class RecordingCapture;

//! A stream: the device, what it plays, and what it records if it does
/*!
 * What plays is rendered by a subclass, which also says where the device is in the timeline. This class does the rest:
 * the device and its callback, recording, the playback volume, the main meter, and reporting what plays.
 */
class PlaybackStream : public juce::AudioIODeviceCallback
{
public:
    //! The stream reports what it plays to `callbackInfos` and `playedRanges`, and applies `playbackVolume`
    PlaybackStream(AudacityProject& project, int token, std::unique_ptr<juce::AudioIODevice> device, bool followsPlayRegion,
                   LockFreeQueue<audio::AudioCallbackInfo>& callbackInfos, LockFreeQueue<PlayedRange>& playedRanges,
                   const std::atomic<float>& playbackVolume);
    //! Subclasses stop() in their destructor, as it releases what they render with
    ~PlaybackStream() override;

    double sampleRate() const { return m_sampleRate; }
    int64_t toSamples(double time) const { return static_cast<int64_t>(std::llround(time * m_sampleRate)); }
    double toTime(int64_t samples) const { return samples / m_sampleRate; }

    //! @param leadInEndTime where the lead-in sequences stop playing, if recording with a lead-in
    virtual void build(const TransportSequences& sequences, const audio::IAudioMeterPtr& meter, std::optional<double> leadInEndTime) = 0;
    //! Records the input; call before start()
    void setCapture(std::unique_ptr<RecordingCapture> capture);
    const RecordingCapture* capture() const { return m_capture.get(); }

    //! Plays from `startTime` to `endTime`, or loops `loopRange` if given, rolling into it if `startTime` is left of it.
    virtual void setPlayRange(double startTime, double endTime, std::optional<std::pair<double, double> > loopRange) = 0;
    //! Processes what renders for its latency, so that the device hears the start of the play range first.
    //! Call after setPlayRange() and before start().
    virtual void preRoll() {}

    //! Main thread, while playing: the play region changed
    virtual void updateLoop(std::optional<std::pair<double, double> > loopRange, double endTime) = 0;

    //! Where in the timeline the device is
    virtual double position() const = 0;
    virtual void seek(double time) = 0;
    void setPaused(bool paused) { m_paused = paused; }
    bool isFinished() const { return m_finished; }

    void start();
    void stop();

    RealtimeEffects::InitializationScope* effectsInitialization();

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
                                          int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext&) final;
    void audioDeviceAboutToStart(juce::AudioIODevice*) final {}
    void audioDeviceStopped() final {}
    void audioDeviceError(const juce::String& errorMessage) final;

    AudacityProject& project;
    const int token;
    //! Whether loop play follows the play region, as with the default playback policy
    const bool followsPlayRegion;
    double trackEndTime = 0.0;

protected:
    //! In logs
    virtual const char* name() const = 0;
    //! Before releaseRendering(): more of the dropouts, for the log, if any
    virtual std::string dropoutDetails() const { return {}; }
    //! Device thread, while not paused: renders `frames` frames into `output` from `offset` on, at most the block size.
    //! Reports what plays, with reportPlayed() and finish(). Returns how many of the frames, at the end, it couldn't
    //! play, being silent instead: the transport doesn't advance by them.
    virtual int renderChunk(float* const* output, int numOutputChannels, int offset, int frames, Clock::time_point dacTime) = 0;
    //! Once the device stopped: releases what renders
    virtual void releaseRendering() = 0;

    //! Before rendering: realtime effects at the rate of the stream, for `sequences` and the master
    void initializeEffects(const ConstPlayableSequences& sequences);
    //! How long after the callback what it outputs is heard
    Clock::duration outputLatency() const;

    // Device thread
    //! `frames` frames from `start` in the timeline play
    void reportPlayed(int64_t start, int64_t frames);
    //! Play reached the end
    void finish() { m_finished = true; }
    //! The meter shows `stereo`, the mix before the playback volume
    void sendToMainMeter(const float* const* stereo, int frames, Clock::time_point dacTime);
    //! `stereo` goes out with the playback volume; a mono device gets the downmix
    void writeOutput(const float* const* stereo, float* const* output, int numOutputChannels, int offset, int frames) const;

    const std::weak_ptr<AudacityProject> m_wProject;
    const double m_sampleRate;
    const int m_blockSize;
    audio::IAudioMeterPtr m_meter;
    ConstPlayableSequences m_sequences;
    std::optional<RealtimeEffects::InitializationScope> m_effectsInitialization;
    //! For what renders; written only by the thread that renders, once rendering
    RenderContext m_context;

private:
    LockFreeQueue<audio::AudioCallbackInfo>& m_callbackInfos;
    LockFreeQueue<PlayedRange>& m_playedRanges;
    const std::atomic<float>& m_playbackVolume;

    std::unique_ptr<juce::AudioIODevice> m_device;
    std::unique_ptr<RecordingCapture> m_capture;

    std::atomic<bool> m_paused { false };
    std::atomic<bool> m_finished { false };

    //! Playback dropouts, to compare the audio engines: written by the callback, read once the device stopped
    struct Dropouts {
        //! What the callback output while playing
        int64_t frames = 0;
        //! Callbacks that took longer than the audio they output
        int lateCallbacks = 0;
        double longestCallback = 0.0;
        //! The block size of the longest callback
        double longestCallbackBlock = 0.0;
    };

    // Device thread
    Dropouts m_dropouts;
    std::vector<float> m_interleaved;
};
}
