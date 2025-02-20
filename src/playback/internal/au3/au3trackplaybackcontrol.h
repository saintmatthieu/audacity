/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "itrackplaybackcontrol.h"
#include "modularity/ioc.h"
#include "context/iglobalcontext.h"
#include "trackedit/iprojecthistory.h"

#include "au3wrap/au3types.h"

namespace au::playback {
using au::audio::volume_dbfs_t;
using au::audio::balance_t;

class Au3TrackPlaybackControl : public ITrackPlaybackControl
{
    muse::Inject<au::context::IGlobalContext> globalContext;
    muse::Inject<au::trackedit::IProjectHistory> projectHistory;

public:
    Au3TrackPlaybackControl() = default;
    volume_dbfs_t volume(long trackId) override;
    void setVolume(long trackId, volume_dbfs_t vol) override;

    balance_t balance(long trackId) override;
    void setBalance(long trackId, balance_t balance) override;


    void setSolo(long trackId, bool solo) override;
    bool solo(long trackId) override;

    void setMuted(long trackId, bool mute) override;
    bool muted(long trackId) override;

private:
    au3::Au3Project& projectRef() const;
};
}
