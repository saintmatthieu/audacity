#include "playbackstate.h"

using namespace au::context;
using namespace au::playback;

void PlaybackState::setPlayer(playback::IPlayerPtr player)
{
    if (m_player) {
        m_player->playbackStatusChanged().resetOnReceive(this);
    }

    m_player = player;

    //! The redirect is needed so that consumers do not have to worry about resubscribing if the player changes
    m_player->playbackStatusChanged().onReceive(this, [this](PlaybackStatus st) {
        m_playbackStatusChanged.send(st);
    });
}

PlaybackStatus PlaybackState::playbackStatus() const
{
    return m_player ? m_player->playbackStatus() : PlaybackStatus::Stopped;
}

bool PlaybackState::isPlaying() const
{
    return playbackStatus() == playback::PlaybackStatus::Running;
}

muse::async::Channel<PlaybackStatus> PlaybackState::playbackStatusChanged() const
{
    return m_playbackStatusChanged;
}

muse::secs_t PlaybackState::playbackPosition() const
{
    return m_player ? m_player->playbackPosition() : muse::secs_t(0.0);
}

void PlaybackState::addPlaybackPositionListener(class playback::IPlaybackPositionListener* listener)
{
    IF_ASSERT_FAILED(m_player) {
        return;
    }
    m_player->addPlaybackPositionListener(listener);
}

void PlaybackState::removePlaybackPositionListener(class playback::IPlaybackPositionListener* listener)
{
    IF_ASSERT_FAILED(m_player) {
        return;
    }
    m_player->removePlaybackPositionListener(listener);
}
