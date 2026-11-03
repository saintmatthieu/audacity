/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "audio/iaudioengine.h"

namespace au::au3audio {
//! The audio engine of the Tracktion backend.
//! TODO: so far a placeholder that never starts a stream.
class TracktionAudioEngine final : public audio::IAudioEngine
{
public:
    bool isBusy() const override;
    bool isCapturing() const override;
    bool isMonitoring() const override;
    std::optional<audio::AudioStreamDescriptor> currentStream() const override;
    bool isStreamActive(AudacityProject& project) const override;
    double transportTime() const override;

    int startTransport(const TransportSequences& sequences, double startTime, double endTime, double mixerEndTime,
                       AudacityProject& project, const StartTransportOptions& options) override;
    void stopTransport() override;
    void pauseTransport(bool pause) override;
    void seek(double time) override;

    std::shared_ptr<RealtimeEffectState> addRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                                const std::string& effectId) override;
    void removeRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                   const std::shared_ptr<RealtimeEffectState>& state) override;
    std::shared_ptr<RealtimeEffectState> replaceRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                                    size_t effectListIndex, const std::string& newEffectId) override;

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
    float m_inputVolume = 1.f;
    float m_playbackVolume = 1.f;

    muse::async::Notification m_recordingUpdateRequested;
    muse::async::Notification m_recordingNewBlocks;
    muse::async::Notification m_recordingCommitRequested;
    muse::async::Notification m_recordingFinished;
    muse::async::Notification m_streamStopped;
    muse::async::Channel<au3::Au3TrackId, au3::Au3ClipId> m_recordingClipChanged;
};
}
