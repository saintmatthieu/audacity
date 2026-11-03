/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <atomic>
#include <map>
#include <memory>

#include "audio/driver/iaudiodrivercontroller.h"
#include "audio/iaudioengine.h"

#include "au3-utility/LockFreeQueue.h"
#include "au3-utility/Observer.h"

#include "playedrange.h"

namespace au::au3audio {
class JuceAudioDevices;
class PlaybackStream;

//! The audio engine of the Tracktion backend: a tracktion_graph with one node per playback
//! sequence, pulled synchronously by the callback of a JUCE audio device.
//! So far playback only: it refuses to record or monitor.
class TracktionAudioEngine final : public audio::IAudioEngine
{
public:
    TracktionAudioEngine(std::shared_ptr<JuceAudioDevices> devices);
    ~TracktionAudioEngine() override;

    //! The controller whose configuration selects the device; must outlive this.
    void setDriverController(const audio::IAudioDriverController* driverController);

    bool isBusy() const override;
    bool isCapturing() const override;
    bool isMonitoring() const override;
    std::optional<audio::AudioStreamDescriptor> currentStream() const override;
    bool isStreamActive(AudacityProject& project) const override;
    double transportTime() const override;

    int startTransport(const TransportSequences& sequences, double startTime, double endTime, double mixerEndTime, AudacityProject& project,
                       const StartTransportOptions& options) override;
    void stopTransport() override;
    void pauseTransport(bool pause) override;
    void seek(double time) override;

    std::shared_ptr<RealtimeEffectState> addRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                                const std::string& effectId) override;
    void removeRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                   const std::shared_ptr<RealtimeEffectState>& state) override;
    std::shared_ptr<RealtimeEffectState> replaceRealtimeEffectState(AudacityProject& project, ChannelGroup* group, size_t effectListIndex,
                                                                    const std::string& newEffectId) override;

    void setPlaybackMeter(AudacityProject& project, const audio::IAudioMeterPtr& meter) override;
    void setCaptureMeter(AudacityProject& project, const audio::IAudioMeterPtr& meter) override;
    void stopMeters(AudacityProject& project) override;

    void startMonitoring(AudacityProject& project) override;
    void stopMonitoring() override;

    void setInputVolume(float newInputVolume) override;
    float getInputVolume() const override;
    void setPlaybackVolume(float newPlaybackVolume) override;
    float getPlaybackVolume() const override;

    bool canStopTransport(AudacityProject& project) const override;

    void handleDeviceChange() override;

    muse::String lastErrorString() const override;
    double getPlaybackSampleRate() const override;
    void updateTimePosition(unsigned long newlyConsumedSamples) override;
    std::optional<audio::AudioCallbackInfo> consumeNextCallbackInfo() override;

    muse::async::Notification recordingUpdateRequested() const override;
    muse::async::Notification recordingNewBlocks() const override;
    muse::async::Notification recordingCommitRequested() const override;
    muse::async::Notification recordingFinished() const override;
    muse::async::Notification streamStopped() const override;
    muse::async::Channel<au3::Au3TrackId, au3::Au3ClipId> recordingClipChanged() const override;

private:
    void onPlayRegionChanged();

    const std::shared_ptr<JuceAudioDevices> m_devices;
    const audio::IAudioDriverController* m_driverController = nullptr;

    std::unique_ptr<PlaybackStream> m_stream;
    int m_nextStreamToken = 1;
    double m_lastSampleRate = 0.0;
    muse::String m_lastError;

    std::atomic<float> m_playbackVolume { 1.f };
    float m_inputVolume = 1.f;

    std::map<const AudacityProject*, audio::IAudioMeterPtr> m_playbackMeters;
    std::map<const AudacityProject*, audio::IAudioMeterPtr> m_captureMeters;

    // Filled by the device callback, consumed by the main thread
    LockFreeQueue<audio::AudioCallbackInfo> m_callbackInfoQueue { 512 };
    LockFreeQueue<PlayedRange> m_playedRanges { 1024 };
    // Main thread state of the consumption of m_playedRanges
    PlayedRange m_currentPlayedRange;
    unsigned long m_consumedInCurrentRange = 0;
    double m_transportTime = 0.0;

    Observer::Subscription m_playRegionSubscription;

    muse::async::Notification m_recordingUpdateRequested;
    muse::async::Notification m_recordingNewBlocks;
    muse::async::Notification m_recordingCommitRequested;
    muse::async::Notification m_recordingFinished;
    muse::async::Notification m_streamStopped;
    muse::async::Channel<au3::Au3TrackId, au3::Au3ClipId> m_recordingClipChanged;
};
}
