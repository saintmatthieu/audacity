/*
* Audacity: A Digital Audio Editor
*/

#pragma once

#include "au3audio/audiotypes.h"
#include "modularity/imoduleinterface.h"

namespace au::playback {
class ITrackPlaybackControl : MODULE_EXPORT_INTERFACE
{
    INTERFACE_ID(ITrackPlaybackControl)

public:
    virtual ~ITrackPlaybackControl() = default;

    virtual void setVolume(long trackId, au::audio::volume_dbfs_t volume) = 0;
    virtual au::audio::volume_dbfs_t volume(long trackId) = 0;

    virtual void setBalance(long trackId, au::audio::balance_t balance) = 0;
    virtual au::audio::balance_t balance(long trackId) = 0;

    virtual void setSolo(long trackId, bool solo) = 0;
    virtual bool solo(long trackId) = 0;

    virtual void setMuted(long trackId, bool mute) = 0;
    virtual bool muted(long trackId) = 0;
};
}
