/*
* Audacity: A Digital Audio Editor
*/
#include "tracktionaudioengine.h"

#include <algorithm>
#include <optional>

#include "framework/global/containers.h"
#include "framework/global/log.h"

#include "au3-project/Project.h"
#include "au3-realtime-effects/RealtimeEffectManager.h"
#include "au3-time-frequency-selection/ViewInfo.h"
#include "au3-track/Track.h"

#include "au3wrap/internal/wxtypes_convert.h"

#include "juceaudiodevices.h"
#include "playbackstream.h"

using namespace au::au3audio;

namespace {
//! How far right of the loop region the play head may be for loop play to still capture it (as in DefaultPlaybackPolicy)
constexpr double loopCaptureAllowance = 0.5;

std::string effectiveOutputDevice(const JuceAudioDevices& devices, const au::audio::AudioConfiguration& configuration)
{
    if (configuration.outputDevice && muse::contains(devices.outputDevices(configuration.api), *configuration.outputDevice)) {
        return *configuration.outputDevice;
    }
    return devices.systemDefaultOutputDevice(configuration.api);
}

//! The requested rate if the device supports it, else its highest
double bestSampleRate(juce::AudioIODevice& device, double requested)
{
    const auto rates = device.getAvailableSampleRates();
    if (rates.isEmpty() || rates.contains(requested)) {
        return requested;
    }
    return *std::max_element(rates.begin(), rates.end());
}
}

TracktionAudioEngine::TracktionAudioEngine(std::shared_ptr<JuceAudioDevices> devices)
    : m_devices{std::move(devices)}
{
}

TracktionAudioEngine::~TracktionAudioEngine() = default;

void TracktionAudioEngine::setDriverController(const audio::IAudioDriverController* driverController)
{
    m_driverController = driverController;
}

bool TracktionAudioEngine::isBusy() const
{
    return m_stream != nullptr;
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
    if (!m_stream) {
        return std::nullopt;
    }
    return audio::AudioStreamDescriptor { audio::AudioStreamKind::Playback, &m_stream->project, m_stream->sampleRate() };
}

bool TracktionAudioEngine::isStreamActive(AudacityProject& project) const
{
    return m_stream && &m_stream->project == &project && !m_stream->isFinished();
}

double TracktionAudioEngine::transportTime() const
{
    return m_transportTime;
}

int TracktionAudioEngine::startTransport(const TransportSequences& sequences, double startTime, double endTime,
                                         double /*mixerEndTime*/, AudacityProject& project, const StartTransportOptions& options)
{
    if (m_stream) {
        return 0;
    }
    if (!sequences.captureSequences.empty()) {
        m_lastError = u"The Tracktion audio engine cannot record yet";
        return 0;
    }
    if (sequences.playbackSequences.empty()) {
        m_lastError = u"Nothing to play";
        return 0;
    }
    // The graphs are read rather than the sequences
    IF_ASSERT_FAILED(sequences.playbackGraphs.size() == sequences.playbackSequences.size()
                     && std::all_of(sequences.playbackGraphs.begin(), sequences.playbackGraphs.end(),
                                    [](const auto& graph) { return graph != nullptr; })) {
        return 0;
    }
    IF_ASSERT_FAILED(m_driverController) {
        return 0;
    }

    const auto configuration = m_driverController->configuration();
    const auto type = m_devices->deviceType(configuration.api);
    if (!type) {
        m_lastError = muse::String::fromStdString("No such audio API: " + configuration.api);
        return 0;
    }
    const auto outputDevice = effectiveOutputDevice(*m_devices, configuration);
    std::unique_ptr<juce::AudioIODevice> device { type->createDevice(juce::String(outputDevice), {}) };
    if (!device) {
        m_lastError = muse::String::fromStdString("Cannot create audio device " + outputDevice);
        return 0;
    }

    juce::BigInteger outputChannels;
    outputChannels.setRange(0, std::min(numChannels, device->getOutputChannelNames().size()), true);
    const auto sampleRate = bestSampleRate(*device, options.sampleRate);
    const auto error = device->open({}, outputChannels, sampleRate, device->getDefaultBufferSize());
    if (error.isNotEmpty()) {
        m_lastError = muse::String::fromStdString(error.toStdString());
        return 0;
    }

    // As AudioIO: the default policy plays from the play region start, and loops it if it is active
    const auto& playRegion = ViewInfo::Get(project).playRegion;
    std::optional<double> streamStartTime = options.streamStartTime;
    if (!streamStartTime && options.isDefaultPolicy) {
        streamStartTime = std::max(0.0, playRegion.GetStart());
    }
    const double playStartTime = streamStartTime.value_or(startTime);

    auto stream = std::make_unique<PlaybackStream>(project, m_nextStreamToken, std::move(device), options.isDefaultPolicy,
                                                   m_callbackInfoQueue, m_playedRanges, m_playbackVolume);
    stream->trackEndTime = TrackList::Get(project).GetEndTime();
    const auto meter = muse::value(m_playbackMeters, &project);
    // Sequences are read as far as the play head goes, which is past the end of the play range only if loop play is turned
    // off while playing: then to the end of the tracks
    stream->build(sequences, meter);

    std::optional<std::pair<double, double> > loopRange;
    if (options.isDefaultPolicy && playRegion.Active() && startTime < endTime
        && playStartTime <= endTime + loopCaptureAllowance) {
        loopRange.emplace(startTime, endTime);
    }
    stream->setPlayRange(playStartTime, endTime, loopRange);
    stream->preRoll();

    m_callbackInfoQueue.Clear();
    m_playedRanges.Clear();
    m_currentPlayedRange = { playStartTime, 0 };
    m_consumedInCurrentRange = 0;
    m_transportTime = playStartTime;
    m_lastSampleRate = stream->sampleRate();

    m_stream = std::move(stream);
    if (m_stream->followsPlayRegion) {
        m_playRegionSubscription = ViewInfo::Get(project).playRegion.Subscribe([this](auto&) { onPlayRegionChanged(); });
    }
    m_stream->start();

    LOGI() << "Tracktion engine playing at " << m_lastSampleRate << " Hz on " << outputDevice;
    return m_nextStreamToken++;
}

void TracktionAudioEngine::onPlayRegionChanged()
{
    if (!m_stream) {
        return;
    }
    const auto& playRegion = ViewInfo::Get(m_stream->project).playRegion;
    const double t0 = playRegion.GetStart();
    const double t1 = playRegion.GetEnd();
    std::optional<std::pair<double, double> > loopRange;
    if (playRegion.Active() && t0 < t1 && m_stream->position() <= t1 + loopCaptureAllowance) {
        loopRange.emplace(t0, t1);
    }
    // Without a loop, play on to the end of the tracks
    m_stream->updateLoop(loopRange, std::max(t0, m_stream->trackEndTime));
}

void TracktionAudioEngine::stopTransport()
{
    if (!m_stream) {
        return;
    }
    m_playRegionSubscription.Reset();
    m_stream->stop();
    m_stream.reset();
    m_streamStopped.notify();
}

void TracktionAudioEngine::pauseTransport(bool pause)
{
    if (m_stream) {
        m_stream->setPaused(pause);
    }
}

void TracktionAudioEngine::seek(double time)
{
    if (m_stream) {
        m_stream->seek(time);
    }
}

std::shared_ptr<RealtimeEffectState> TracktionAudioEngine::addRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                                                  const std::string& effectId)
{
    const auto scope = m_stream && &m_stream->project == &project ? m_stream->effectsInitialization() : nullptr;
    return RealtimeEffectManager::Get(project).AddState(scope, group, au3::wxFromStdString(effectId));
}

void TracktionAudioEngine::removeRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                     const std::shared_ptr<RealtimeEffectState>& state)
{
    const auto scope = m_stream && &m_stream->project == &project ? m_stream->effectsInitialization() : nullptr;
    RealtimeEffectManager::Get(project).RemoveState(scope, group, state);
}

std::shared_ptr<RealtimeEffectState> TracktionAudioEngine::replaceRealtimeEffectState(AudacityProject& project, ChannelGroup* group,
                                                                                      size_t effectListIndex,
                                                                                      const std::string& newEffectId)
{
    const auto scope = m_stream && &m_stream->project == &project ? m_stream->effectsInitialization() : nullptr;
    return RealtimeEffectManager::Get(project).ReplaceState(scope, group, effectListIndex, au3::wxFromStdString(newEffectId));
}

void TracktionAudioEngine::setPlaybackMeter(AudacityProject& project, const audio::IAudioMeterPtr& meter)
{
    m_playbackMeters[&project] = meter;
}

void TracktionAudioEngine::setCaptureMeter(AudacityProject& project, const audio::IAudioMeterPtr& meter)
{
    m_captureMeters[&project] = meter;
}

void TracktionAudioEngine::stopMeters(AudacityProject& project)
{
    for (const auto* meters : { &m_playbackMeters, &m_captureMeters }) {
        if (const auto meter = muse::value(*meters, &project)) {
            meter->stop();
        }
    }
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

bool TracktionAudioEngine::canStopTransport(AudacityProject& project) const
{
    return !m_stream || &m_stream->project == &project;
}

void TracktionAudioEngine::handleDeviceChange()
{
    // The device is opened for each stream, and the driver controller suspends streams before changing it
}

muse::String TracktionAudioEngine::lastErrorString() const
{
    return m_lastError;
}

double TracktionAudioEngine::getPlaybackSampleRate() const
{
    return m_lastSampleRate;
}

void TracktionAudioEngine::updateTimePosition(unsigned long newlyConsumedSamples)
{
    while (newlyConsumedSamples > 0) {
        if (m_consumedInCurrentRange == m_currentPlayedRange.frames) {
            PlayedRange next;
            if (!m_playedRanges.Get(next)) {
                break;
            }
            m_currentPlayedRange = next;
            m_consumedInCurrentRange = 0;
        }
        const auto consumed = std::min(newlyConsumedSamples, m_currentPlayedRange.frames - m_consumedInCurrentRange);
        m_consumedInCurrentRange += consumed;
        newlyConsumedSamples -= consumed;
    }
    if (m_lastSampleRate > 0) {
        m_transportTime = m_currentPlayedRange.time + m_consumedInCurrentRange / m_lastSampleRate;
    }
}

std::optional<au::audio::AudioCallbackInfo> TracktionAudioEngine::consumeNextCallbackInfo()
{
    audio::AudioCallbackInfo info;
    if (m_callbackInfoQueue.Get(info)) {
        return info;
    }
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
