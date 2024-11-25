/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "modularity/ioc.h"
#include "async/asyncable.h"
#include "../iaudioengine.h"
#include "effects/effects_base/ieffectsprovider.h"
#include "context/iglobalcontext.h"
#include "trackedit/iprojecthistory.h"
#include "libraries/lib-utility/Observer.h"
#include <unordered_map>
#include <memory>

struct TrackListEvent;

class AudacityProject;
class RealtimeEffectList;
class WaveTrack;
class RealtimeEffectState;

namespace au3 {
using Au3Project = ::AudacityProject;
}

namespace au::audio {
class AudioEngine : public IAudioEngine, muse::async::Asyncable, public std::enable_shared_from_this<AudioEngine>
{
    muse::Inject<context::IGlobalContext> globalContext;
    muse::Inject<trackedit::IProjectHistory> projectHistory;
    muse::Inject<effects::IEffectsProvider> effectsProvider;

public:
    AudioEngine() = default;

    void init();

    bool isBusy() const override;

    int startStream(const TransportSequences& sequences, double startTime, double endTime, double mixerEndTime,
                    const AudioIOStartStreamOptions& options) override;

    void addRealtimeEffect(project::IAudacityProject&, TrackId, const EffectId&) override;
    void removeRealtimeEffect(project::IAudacityProject&, TrackId, EffectState) override;
    void replaceRealtimeEffect(project::IAudacityProject&, TrackId, int effectListIndex, const EffectId& newEffectId) override;

    muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr> realtimeEffectAdded() const override;
    muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr> realtimeEffectRemoved() const override;
    muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr, EffectChainLinkPtr> realtimeEffectReplaced() const override;

    muse::async::Notification updateRequested() const override;
    muse::async::Notification commitRequested() const override;
    muse::async::Notification finished() const override;
    muse::async::Channel<au3::Au3TrackId, au3::Au3ClipId> recordingClipChanged() const override;

private:
    Observer::Subscription subscribeToRealtimeEffectList(WaveTrack&, RealtimeEffectList&);
    void onTrackListEvent(const TrackListEvent&);
    void onWaveTrackAdded(WaveTrack&);
    std::string getEffectName(const std::string& effectId) const;
    std::string getEffectName(const RealtimeEffectState& state) const;
    std::string getTrackName(const au::au3::Au3Project& project, au::audio::TrackId trackId) const;

    muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr> m_realtimeEffectAdded;
    muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr> m_realtimeEffectRemoved;
    muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr, EffectChainLinkPtr> m_realtimeEffectReplaced;

    Observer::Subscription m_tracklistSubscription;
    std::unordered_map<TrackId, Observer::Subscription> m_rtEffectSubscriptions;
};
}
