#include "audioengine.h"

#include "libraries/lib-audio-io/AudioIO.h"
#include "libraries/lib-audio-io/ProjectAudioIO.h"

#include "libraries/lib-wave-track/WaveTrack.h"
#include "libraries/lib-time-frequency-selection/ViewInfo.h"
#include "libraries/lib-effects/EffectManager.h"
#include "libraries/lib-realtime-effects/RealtimeEffectState.h"
#include "libraries/lib-realtime-effects/RealtimeEffectList.h"

#include "au3wrap/au3types.h"
#include "au3wrap/internal/domaccessor.h"
#include "au3wrap/internal/domconverter.h"
#include "au3wrap/iau3project.h"

#include "defaultplaybackpolicy.h"
#include "au3audioiolistener.h"
#include "audiosourcewrapper.h"
#include "project/iaudacityproject.h"

#include "realfn.h"

using namespace au::audio;

std::string AudioEngine::getEffectName(const RealtimeEffectState& state) const
{
    return effectsProvider()->effectName(state.GetID().ToStdString());
}

std::shared_ptr<Au3AudioIOListener> s_audioIOListener;

static ProjectAudioIO::DefaultOptions::Scope s_defaultOptionsScope {
    [](au::au3::Au3Project& project, bool newDefault) -> AudioIOStartStreamOptions {
        auto options = ProjectAudioIO::DefaultOptionsFactory(project, newDefault);
        options.listener = s_audioIOListener;

        auto& playRegion = ViewInfo::Get(project).playRegion;
        bool loopEnabled = playRegion.Active();
        options.loopEnabled = loopEnabled;

        if (newDefault) {
            const double trackEndTime = TrackList::Get(project).GetEndTime();
            const double loopEndTime = playRegion.GetEnd();
            options.policyFactory = [&project, trackEndTime, loopEndTime](
                const AudioIOStartStreamOptions& options) -> std::unique_ptr<PlaybackPolicy>
            {
                return std::make_unique<DefaultPlaybackPolicy>(project,
                                                               trackEndTime, loopEndTime, options.pStartTime,
                                                               options.loopEnabled, options.variableSpeed);
            };

            double startTime = playRegion.GetStart();
            options.pStartTime.emplace(muse::RealIsEqualOrMore(startTime, 0.0) ? startTime : 0.0);
        }

        return options;
    } };

void AudioEngine::init()
{
    s_audioIOListener = std::make_shared<Au3AudioIOListener>();
    globalContext()->currentProjectChanged().onNotify(this, [this] {
        m_rtEffectSubscriptions.clear();
        auto project = globalContext()->currentProject();
        if (!project) {
            return;
        }
        auto au3Project = reinterpret_cast<au::au3::Au3Project*>(project->au3ProjectPtr());
        m_tracklistSubscription = TrackList::Get(*au3Project).Subscribe([this, w = weak_from_this()](const TrackListEvent& e) {
            const auto lifeguard = w.lock();
            if (lifeguard) {
                onTrackListEvent(e);
            }
        });
    });
}

void AudioEngine::onTrackListEvent(const TrackListEvent& e)
{
    auto waveTrack = std::dynamic_pointer_cast<WaveTrack>(e.mpTrack.lock());
    if (!waveTrack) {
        return;
    }
    switch (e.mType) {
    case TrackListEvent::ADDITION:
        onWaveTrackAdded(*waveTrack);
        break;
    case TrackListEvent::DELETION:
        m_rtEffectSubscriptions.erase(waveTrack->GetId());
        break;
    }
}

void AudioEngine::onWaveTrackAdded(WaveTrack& track)
{
    auto& list = RealtimeEffectList::Get(track);

    // We have subscribed for future realtime-effect events, but we need to send the initial state.
    for (auto i = 0u; i < list.GetStatesCount(); ++i) {
        const auto state = list.GetStateAt(i);
        IF_ASSERT_FAILED(state) {
            break;
        }
        auto newEffect = std::make_shared<EffectChainLink>(getEffectName(*state), state.get());
        m_realtimeEffectAdded.send(track.GetId(), i, std::move(newEffect));
    }
}

Observer::Subscription AudioEngine::subscribeToRealtimeEffectList(WaveTrack& track, RealtimeEffectList& list)
{
    return list.Subscribe([&, weakThis = weak_from_this(), weakTrack = track.weak_from_this()](const RealtimeEffectListMessage& msg)
    {
        const auto lifeguard = weakThis.lock();
        if (!lifeguard) {
            return;
        }
        const auto waveTrack = std::dynamic_pointer_cast<WaveTrack>(weakTrack.lock());
        if (!waveTrack) {
            return;
        }
        const auto trackId = waveTrack->GetId();
        auto& list = RealtimeEffectList::Get(*waveTrack);
        switch (msg.type) {
        case RealtimeEffectListMessage::Type::Insert:
        {
            auto newEffect = std::make_shared<EffectChainLink>(getEffectName(*msg.affectedState), msg.affectedState.get());
            m_realtimeEffectAdded.send(trackId, msg.srcIndex, std::move(newEffect));
        }
        break;
        case RealtimeEffectListMessage::Type::Remove:
        {
            auto oldEffect = std::make_shared<EffectChainLink>(getEffectName(*msg.affectedState), msg.affectedState.get());
            m_realtimeEffectRemoved.send(trackId, msg.srcIndex, std::move(oldEffect));
        }
        break;
        case RealtimeEffectListMessage::Type::DidReplace:
        {
            const std::shared_ptr<RealtimeEffectState> newState = list.GetStateAt(msg.dstIndex);
            IF_ASSERT_FAILED(newState) {
                return;
            }
            auto oldEffect = std::make_shared<EffectChainLink>(getEffectName(*msg.affectedState), msg.affectedState.get());
            auto newEffect = std::make_shared<EffectChainLink>(getEffectName(*newState), newState.get());
            m_realtimeEffectReplaced.send(trackId, msg.srcIndex, std::move(oldEffect), std::move(newEffect));
        }
        break;
        }
    });
}

bool AudioEngine::isBusy() const
{
    return AudioIO::Get()->IsBusy();
}

int AudioEngine::startStream(const TransportSequences& sequences, double startTime, double endTime, double mixerEndTime,
                             const AudioIOStartStreamOptions& options)
{
    return AudioIO::Get()->StartStream(sequences, startTime, endTime, mixerEndTime, options);
}

muse::async::Notification AudioEngine::updateRequested() const
{
    return s_audioIOListener->updateRequested();
}

muse::async::Notification AudioEngine::commitRequested() const
{
    return s_audioIOListener->commitRequested();
}

muse::async::Notification AudioEngine::finished() const
{
    return s_audioIOListener->finished();
}

muse::async::Channel<au::au3::Au3TrackId, au::au3::Au3ClipId> AudioEngine::recordingClipChanged() const
{
    return s_audioIOListener->recordingClipChanged();
}

muse::async::Channel<au::audio::TrackId, EffectChainLinkIndex, EffectChainLinkPtr> AudioEngine::realtimeEffectAdded() const
{
    return m_realtimeEffectAdded;
}

muse::async::Channel<au::audio::TrackId, EffectChainLinkIndex, EffectChainLinkPtr> AudioEngine::realtimeEffectRemoved() const
{
    return m_realtimeEffectRemoved;
}

muse::async::Channel<au::audio::TrackId, EffectChainLinkIndex, EffectChainLinkPtr,
                     EffectChainLinkPtr> AudioEngine::realtimeEffectReplaced() const
{
    return m_realtimeEffectReplaced;
}

namespace {
std::pair<au::au3::Au3Project*, au::au3::Au3Track*> au3ProjectAndTrack(au::project::IAudacityProject& project, au::audio::TrackId trackId)
{
    auto au3Project = reinterpret_cast<au::au3::Au3Project*>(project.au3ProjectPtr());
    auto au3Track = dynamic_cast<au::au3::Au3Track*>(au::au3::DomAccessor::findTrack(*au3Project, au::au3::Au3TrackId(trackId)));
    return { au3Project, au3Track };
}
}

void AudioEngine::addRealtimeEffect(au::project::IAudacityProject& project, TrackId trackId, const muse::String& effectId)
{
    const auto [au3Project, au3Track] = au3ProjectAndTrack(project, trackId);
    IF_ASSERT_FAILED(au3Project && au3Track) {
        return;
    }

    if (const auto state = AudioIO::Get()->AddState(*au3Project, au3Track, effectId.toStdString())) {
        const auto effectName = getEffectName(*state);
        const auto trackName =  project.trackeditProject()->trackName(trackId);
        projectHistory()->pushHistoryState("Added " + effectName + " to " + trackName, "Add " + effectName);
    }
}

void AudioEngine::registerAudioSource(au::au3::IAu3Project& project, IAudioSource* source)
{
    auto wrapper = std::make_shared<AudioSourceWrapper>(source, m_realtimeEffectAppended);
    // auto& list = RealtimeEffectList::Get(*wrapper);
    // m_rtEffectSubscriptions[track.GetId()] = subscribeToRealtimeEffectList(track, list);
    m_sources[&project].emplace_back(std::move(wrapper));
}

void AudioEngine::unregisterAudioSource(const IAudioSource* source)
{
    for (auto& [project, sources] : m_sources) {
        const auto it = std::find_if(sources.begin(), sources.end(), [source](const auto& wrapper) {
            return wrapper->source() == source;
        });
        if (it != sources.end()) {
            sources.erase(it);
            if (sources.empty()) {
                m_sources.erase(project);
            }
        }
    }
}

void AudioEngine::appendRealtimeEffect(const IAudioSource* source, const std::string& effectId)
{
    for (const auto& [project, sources] : m_sources) {
        const auto it = std::find_if(sources.begin(), sources.end(), [source](const auto& wrapper) {
            return wrapper->source() == source;
        });
        if (it != sources.end()) {
            auto au3Project = reinterpret_cast<au::au3::Au3Project*>(project->au3ProjectPtr());
            AudioIO::Get()->AddState(*au3Project, (*it).get(), effectId);
            break;
        }
    }
}

muse::async::Channel<const IAudioSource*, EffectChainLinkIndex, EffectChainLinkPtr> AudioEngine::realtimeEffectAppended() const
{
    return m_realtimeEffectAppended;
}

void AudioEngine::removeRealtimeEffect(au::project::IAudacityProject& project, TrackId trackId, au::audio::EffectState effectState)
{
    const auto [au3Project, au3Track] = au3ProjectAndTrack(project, trackId);
    IF_ASSERT_FAILED(au3Project && au3Track) {
        return;
    }

    const auto state = reinterpret_cast<RealtimeEffectState*>(effectState)->shared_from_this();
    AudioIO::Get()->RemoveState(*au3Project, au3Track, state);
    const auto effectName = getEffectName(*state);
    const auto trackName = project.trackeditProject()->trackName(trackId);
    projectHistory()->pushHistoryState("Removed " + effectName + " from " + trackName, "Remove " + effectName);
}

void AudioEngine::replaceRealtimeEffect(au::project::IAudacityProject& project, TrackId trackId, int effectListIndex,
                                        const muse::String& newEffectId)
{
    const auto [au3Project, au3Track] = au3ProjectAndTrack(project, trackId);
    IF_ASSERT_FAILED(au3Project && au3Track) {
        return;
    }

    const auto newState = AudioIO::Get()->ReplaceState(*au3Project, au3Track, effectListIndex, newEffectId.toStdString());
    const auto oldState = RealtimeEffectList::Get(*au3Track).GetStateAt(effectListIndex);
    if (newState && oldState) {
        const auto newEffectName = getEffectName(*newState);
        const auto oldEffectName = getEffectName(*oldState);
        projectHistory()->pushHistoryState("Replaced " + oldEffectName + " with " + newEffectName, "Replace " + oldEffectName);
    }
}

// Inject a factory for realtime effects
static RealtimeEffectState::EffectFactory::Scope scope{ &EffectManager::GetInstanceFactory };
