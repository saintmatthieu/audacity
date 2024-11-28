/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "async/notification.h"
#include "async/channel.h"

#include "global/modularity/imoduleinterface.h"

#include "audioenginetypes.h"
#include "iaudiosource.h"
#include "au3wrap/au3types.h"

struct TransportSequences;
struct AudioIOStartStreamOptions;

namespace au::project {
class IAudacityProject;
}

namespace au::au3 {
class IAu3Project;
}

namespace au::audio {
class IAudioEngine : MODULE_EXPORT_INTERFACE
{
    INTERFACE_ID(IAudioEngine);
public:
    virtual ~IAudioEngine() = default;

    virtual bool isBusy() const = 0;

    virtual int startStream(const TransportSequences& sequences, double startTime, double endTime, double mixerEndTime, // Time at which mixer stops producing, maybe > endTime
                            const AudioIOStartStreamOptions& options) = 0;

    virtual void addRealtimeEffect(project::IAudacityProject& project, TrackId trackId, const EffectId& effectId) = 0;
    virtual void removeRealtimeEffect(project::IAudacityProject& project, TrackId trackId, EffectState state) = 0;
    virtual void replaceRealtimeEffect(project::IAudacityProject& project, TrackId trackId, int effectListIndex,
                                       const EffectId& newEffectId) = 0;

    virtual void registerAudioSource(au::au3::IAu3Project&, IAudioSource*) = 0;
    virtual void unregisterAudioSource(const IAudioSource*) = 0;

    virtual void appendRealtimeEffect(const IAudioSource*, const std::string& effectId) = 0;
    virtual muse::async::Channel<const IAudioSource*, EffectChainLinkIndex, EffectChainLinkPtr> realtimeEffectAppended() const = 0;

    virtual muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr> realtimeEffectAdded() const = 0;
    virtual muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr> realtimeEffectRemoved() const = 0;
    virtual muse::async::Channel<TrackId, EffectChainLinkIndex, EffectChainLinkPtr, EffectChainLinkPtr> realtimeEffectReplaced() const = 0;

    virtual muse::async::Notification updateRequested() const = 0;
    virtual muse::async::Notification commitRequested() const = 0;
    virtual muse::async::Notification finished() const = 0;
    virtual muse::async::Channel<au3::Au3TrackId, au3::Au3ClipId> recordingClipChanged() const = 0;
};
}
