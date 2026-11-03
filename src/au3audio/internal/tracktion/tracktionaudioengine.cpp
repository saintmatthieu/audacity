/*
* Audacity: A Digital Audio Editor
*/
#include "tracktionaudioengine.h"

using namespace au::au3audio;

bool TracktionAudioEngine::isBusy() const
{
    return false;
}

bool TracktionAudioEngine::isCapturing() const
{
    return false;
}

bool TracktionAudioEngine::isMonitoring() const
{
    return false;
}

std::optional<au::audio::AudioStreamDescriptor> TracktionAudioEngine::currentStream() const
{
    return std::nullopt;
}

bool TracktionAudioEngine::isStreamActive(AudacityProject&) const
{
    return false;
}

double TracktionAudioEngine::transportTime() const
{
    return 0.0;
}

int TracktionAudioEngine::startTransport(const TransportSequences&, double, double, double, AudacityProject&,
                                         const StartTransportOptions&)
{
    return 0;
}

void TracktionAudioEngine::stopTransport()
{
}

void TracktionAudioEngine::pauseTransport(bool)
{
}

void TracktionAudioEngine::seek(double)
{
}

std::shared_ptr<RealtimeEffectState> TracktionAudioEngine::addRealtimeEffectState(AudacityProject&, ChannelGroup*,
                                                                                  const std::string&)
{
    return nullptr;
}

void TracktionAudioEngine::removeRealtimeEffectState(AudacityProject&, ChannelGroup*, const std::shared_ptr<RealtimeEffectState>&)
{
}

std::shared_ptr<RealtimeEffectState> TracktionAudioEngine::replaceRealtimeEffectState(AudacityProject&, ChannelGroup*, size_t,
                                                                                      const std::string&)
{
    return nullptr;
}

void TracktionAudioEngine::setPlaybackMeter(AudacityProject&, const audio::IAudioMeterPtr&)
{
}

void TracktionAudioEngine::setCaptureMeter(AudacityProject&, const audio::IAudioMeterPtr&)
{
}

void TracktionAudioEngine::stopMeters(AudacityProject&)
{
}

void TracktionAudioEngine::startMonitoring(AudacityProject&)
{
}

void TracktionAudioEngine::stopMonitoring()
{
}

void TracktionAudioEngine::setInputVolume(float newInputVolume)
{
    m_inputVolume = newInputVolume;
}

float TracktionAudioEngine::getInputVolume() const
{
    return m_inputVolume;
}

void TracktionAudioEngine::setPlaybackVolume(float newPlaybackVolume)
{
    m_playbackVolume = newPlaybackVolume;
}

float TracktionAudioEngine::getPlaybackVolume() const
{
    return m_playbackVolume;
}

bool TracktionAudioEngine::canStopTransport(AudacityProject&) const
{
    return true;
}

void TracktionAudioEngine::handleDeviceChange()
{
}

muse::String TracktionAudioEngine::lastErrorString() const
{
    return u"The Tracktion audio engine is not implemented yet";
}

double TracktionAudioEngine::getPlaybackSampleRate() const
{
    return 0.0;
}

void TracktionAudioEngine::updateTimePosition(unsigned long)
{
}

std::optional<au::audio::AudioCallbackInfo> TracktionAudioEngine::consumeNextCallbackInfo()
{
    return std::nullopt;
}

muse::async::Notification TracktionAudioEngine::recordingUpdateRequested() const
{
    return m_recordingUpdateRequested;
}

muse::async::Notification TracktionAudioEngine::recordingNewBlocks() const
{
    return m_recordingNewBlocks;
}

muse::async::Notification TracktionAudioEngine::recordingCommitRequested() const
{
    return m_recordingCommitRequested;
}

muse::async::Notification TracktionAudioEngine::recordingFinished() const
{
    return m_recordingFinished;
}

muse::async::Notification TracktionAudioEngine::streamStopped() const
{
    return m_streamStopped;
}

muse::async::Channel<au::au3::Au3TrackId, au::au3::Au3ClipId> TracktionAudioEngine::recordingClipChanged() const
{
    return m_recordingClipChanged;
}
