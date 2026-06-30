/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "actions/tests/mocks/actionsdispatchermock.h"
#include "context/tests/mocks/globalcontextmock.h"
#include "global/tests/mocks/applicationmock.h"
#include "mocks/playbackmock.h"
#include "mocks/playermock.h"
#include "project/tests/mocks/audacityprojectmock.h"
#include "record/tests/mocks/recordcontrollermock.h"
#include "trackedit/tests/mocks/selectioncontrollermock.h"
#include "trackedit/tests/mocks/trackeditprojectmock.h"

#include "../internal/playbackcontroller.h"
#include "../internal/transport.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

using namespace muse;
using namespace au;
using namespace au::playback;
using namespace au::context;

static const actions::ActionQuery PLAYBACK_SEEK_QUERY("action://playback/seek");
static const actions::ActionQuery PLAYBACK_CHANGE_PLAY_REGION_QUERY("action://playback/play-region-change");

namespace au::playback {
//! The transport owns the playback session state and the state machine; it
//! delegates the actual stream operations to IPlayer and reads recording state
//! from IRecordController. Because both collaborators are interfaces, the whole
//! state machine is unit-testable with mocks — no au3 globals. The tests drive
//! the intents through the (thin) PlaybackController and assert on the player mock.
class TransportTests : public ::testing::Test
{
public:
    void SetUp() override
    {
        m_application = std::make_shared<NiceMock<muse::ApplicationMock> >();
        m_globalContext = std::make_shared<NiceMock<context::GlobalContextMock> >();
        m_dispatcher = std::make_shared<NiceMock<actions::ActionsDispatcherMock> >();
        m_recordController = std::make_shared<NiceMock<record::RecordControllerMock> >();
        m_selectionController = std::make_shared<NiceMock<trackedit::SelectionControllerMock> >();
        m_trackeditProject = std::make_shared<NiceMock<trackedit::TrackeditProjectMock> >();
        m_currentProject = std::make_shared<NiceMock<project::AudacityProjectMock> >();
        m_playback = std::make_shared<NiceMock<PlaybackMock> >();
        m_player = std::make_shared<NiceMock<PlayerMock> >();

        EXPECT_CALL(*m_playback, player(_))
        .WillRepeatedly(Return(m_player));

        ON_CALL(*m_globalContext, currentProject())
        .WillByDefault(Return(m_currentProject));

        ON_CALL(*m_currentProject, trackeditProject())
        .WillByDefault(Return(m_trackeditProject));

        ON_CALL(*m_trackeditProject, totalTime())
        .WillByDefault(Return(100));

        EXPECT_CALL(*m_recordController, isRecording())
        .WillRepeatedly(Return(false));

        // The transport owns the playback state and logic; the controller is the
        // thin action listener that forwards to it. The tests drive the actions
        // through the controller (helpers below) and assert on the player mock.
        m_transport = std::make_shared<Transport>(muse::modularity::globalCtx());
        m_transport->globalContext.set(m_globalContext);
        m_transport->playback.set(m_playback);
        m_transport->recordController.set(m_recordController);
        m_transport->selectionController.set(m_selectionController);
        m_transport->init();

        m_controller = std::make_shared<PlaybackController>(muse::modularity::globalCtx());
        m_controller->transport.set(m_transport);
        m_controller->playback.set(m_playback);
        m_controller->application.set(m_application);
        m_controller->globalContext.set(m_globalContext);
        m_controller->dispatcher.set(m_dispatcher);
        m_controller->recordController.set(m_recordController);
        m_controller->init();
    }

    void togglePlay()
    {
        m_controller->togglePlayAction();
    }

    void changePlaybackRegion(const secs_t start, const secs_t end)
    {
        muse::actions::ActionQuery q(PLAYBACK_CHANGE_PLAY_REGION_QUERY);
        q.addParam("start", muse::Val(start));
        q.addParam("end", muse::Val(end));
        m_controller->onChangePlaybackRegionAction(q);
    }

    void seek(const secs_t seekTime, const bool triggerPlay = false)
    {
        muse::actions::ActionQuery q(PLAYBACK_SEEK_QUERY);
        q.addParam("seekTime", muse::Val(seekTime));
        q.addParam("triggerPlay", muse::Val(triggerPlay));
        m_controller->onSeekAction(q);
    }

    void rewindToStart()
    {
        m_controller->rewindToStartAction();
    }

    void rewindToEnd()
    {
        m_controller->rewindToEndAction();
    }

    void pause()
    {
        m_controller->pauseAction();
    }

    void withStreamRestart(const std::function<void()>& action)
    {
        m_transport->withStreamRestart(action);
    }

    //! Makes the player mock behave like a small transport state machine and
    //! records the order in which stop/play/pause happen, so device-change
    //! orchestration can be asserted as an ordered sequence of events.
    void setupStatefulTransport(PlaybackStatus initial)
    {
        m_status = initial;
        m_events.clear();

        ON_CALL(*m_player, playbackStatus())
        .WillByDefault(::testing::Invoke([this]() { return m_status; }));
        ON_CALL(*m_player, stop())
        .WillByDefault(::testing::Invoke([this]() { m_status = PlaybackStatus::Stopped; m_events.push_back("stop"); }));
        ON_CALL(*m_player, play())
        .WillByDefault(::testing::Invoke([this]() { m_status = PlaybackStatus::Running; m_events.push_back("play"); }));
        ON_CALL(*m_player, pause())
        .WillByDefault(::testing::Invoke([this]() { m_status = PlaybackStatus::Paused; m_events.push_back("pause"); }));
    }

    std::shared_ptr<Transport> m_transport;
    std::shared_ptr<PlaybackController> m_controller;

    PlaybackStatus m_status = PlaybackStatus::Stopped;
    std::vector<std::string> m_events;

    std::shared_ptr<ApplicationMock> m_application;
    std::shared_ptr<context::GlobalContextMock> m_globalContext;
    std::shared_ptr<actions::IActionsDispatcher> m_dispatcher;
    std::shared_ptr<record::RecordControllerMock> m_recordController;
    std::shared_ptr<trackedit::SelectionControllerMock> m_selectionController;
    std::shared_ptr<trackedit::TrackeditProjectMock> m_trackeditProject;
    std::shared_ptr<project::AudacityProjectMock> m_currentProject;

    std::shared_ptr<PlaybackMock> m_playback;
    std::shared_ptr<PlayerMock> m_player;
};

/**
 * @brief Toggle play when stopped without selection or loop
 * @details User clicked play without any additional params
 *          Project has content, no selection, no loop active
 *          Playback should start from current stopped position without seeking
 */
TEST_F(TransportTests, TogglePlay_WhenStopped)
{
    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [GIVEN] Project has content (totalTime = 100.0)
    //! This is set up in SetUp() via m_trackeditProject->totalTime()

    //! [GIVEN] Playback position is at some position (not at the end)
    secs_t currentPosition = 42.0;
    EXPECT_CALL(*m_player, playbackPosition())
    .WillRepeatedly(Return(currentPosition));

    //! [GIVEN] No item selection
    EXPECT_CALL(*m_selectionController, leftMostSelectedItemStartTime())
    .WillOnce(Return(std::nullopt));
    EXPECT_CALL(*m_selectionController, rightMostSelectedItemEndTime())
    .WillOnce(Return(std::nullopt));

    //! [GIVEN] No time selection
    EXPECT_CALL(*m_selectionController, timeSelectionIsEmpty())
    .WillOnce(Return(true));

    //! [GIVEN] No loop region active
    EXPECT_CALL(*m_player, isLoopRegionActive())
    .WillRepeatedly(Return(false));

    //! [THEN] No seek should occur (play from current stopped position)
    EXPECT_CALL(*m_player, seek(_, _))
    .Times(0);

    //! [THEN] Playback region falls back to {lastPlaybackSeekTime, totalPlayTime}.
    //! lastPlaybackSeekTime is 0 (default, no prior seek) and totalPlayTime is 100.
    EXPECT_CALL(*m_player, setPlaybackRegion(PlaybackRegion { secs_t(0.0), secs_t(100.0) }))
    .Times(1);

    //! [THEN] Player should start playing from current position
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Toggle play when stopped on the end of project
 * @details User clicked play after the previous playback reached the end of project
 *          Playback should be started from start of project (0.0 time)
 */
TEST_F(TransportTests, TogglePlay_WhenStopped_OnTheEndOfProject)
{
    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [GIVEN] Was stoped on the end of project
    EXPECT_CALL(*m_player, playbackPosition())
    .WillOnce(Return(secs_t(100.0)));

    //! [THEN] Seek position to start
    EXPECT_CALL(*m_player, seek(secs_t(0.0), false))
    .Times(1);

    //! [THEN] Player should start playing
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Toggle play when there is selection
 * @details User made a selection and clicked play
 *          Playback should be started from selection's start
 */
TEST_F(TransportTests, TogglePlay_WithSelection)
{
    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [GIVEN] There is selection from 10 to 20 secs
    PlaybackRegion selectionRegion = { secs_t(10.0), secs_t(20.0) };
    EXPECT_CALL(*m_selectionController, timeSelectionIsEmpty())
    .WillOnce(Return(false));
    EXPECT_CALL(*m_selectionController, dataSelectedStartTime())
    .WillOnce(Return(selectionRegion.start));
    EXPECT_CALL(*m_selectionController, dataSelectedEndTime())
    .WillOnce(Return(selectionRegion.end));

    //! [THEN] Expect that we will take into account the selection region
    EXPECT_CALL(*m_player, setPlaybackRegion(selectionRegion))
    .Times(1);

    //! [THEN] Player should start playing
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Toggle play when there is clips data selection
 * @details User made a selection by double clicking on waveform and clicked play
 *          Playback should be started from clip's start time
 */
TEST_F(TransportTests, TogglePlay_WithSelection_Clip)
{
    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [GIVEN] Playback position is at the beginning
    EXPECT_CALL(*m_player, playbackPosition())
    .WillRepeatedly(Return(secs_t(0.0)));

    //! [GIVEN] There is single clip selection from 10 to 20 secs
    PlaybackRegion selectionRegion = { secs_t(10.0), secs_t(20.0) };
    EXPECT_CALL(*m_selectionController, leftMostSelectedItemStartTime())
    .WillOnce(Return(std::optional<secs_t>(selectionRegion.start)));
    EXPECT_CALL(*m_selectionController, rightMostSelectedItemEndTime())
    .WillOnce(Return(std::optional<secs_t>(selectionRegion.end)));

    //! [THEN] Expect that we will take into account the clip's selection region
    EXPECT_CALL(*m_player, setPlaybackRegion(selectionRegion))
    .Times(1);

    //! [THEN] No explicit seek (will play from clip start via playback region)
    EXPECT_CALL(*m_player, seek(_, _))
    .Times(0);

    //! [THEN] Player should start playing
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Toggle play with ignore selection
 * @details User clicked play with Shift modifier
 *          Playback should be started from previous seek position
 */
TEST_F(TransportTests, TogglePlay_WithIgnoreSelection)
{
    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [GIVEN] Play with Shift modifier
    ON_CALL(*m_application, keyboardModifiers())
    .WillByDefault(Return(Qt::ShiftModifier));

    //! [THEN] No checking selection
    EXPECT_CALL(*m_selectionController, timeSelectionIsEmpty())
    .Times(0);

    //! [THEN] Expect that playback region will be reseted and playback will be seek to previous seek position
    EXPECT_CALL(*m_player, setPlaybackRegion(PlaybackRegion()))
    .Times(1);
    EXPECT_CALL(*m_player, seek(_, _))
    .Times(1);

    //! [THEN] Player should start playing
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Toggle play when already playing
 * @details User clicked play again for pause
 *          Playback should be paused
 */
TEST_F(TransportTests, TogglePlay_WhenPlaying)
{
    //! [GIVEN] Playback is running
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Running));

    //! [THEN] Player should pause playing
    EXPECT_CALL(*m_player, pause())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

TEST_F(TransportTests, Pause_WhenSeekTargetChangedDuringPlayback_StopsPlayback)
{
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Running));

    EXPECT_CALL(*m_player, setPlaybackRegion(PlaybackRegion()))
    .Times(1);
    EXPECT_CALL(*m_player, stop())
    .Times(1);
    EXPECT_CALL(*m_player, seek(secs_t(12.0), false))
    .Times(1);
    EXPECT_CALL(*m_player, pause())
    .Times(0);

    m_transport->setLastPlaybackSeekTime(12.0);
    pause();
}

TEST_F(TransportTests, Pause_WhenPlaybackRegionChangesAfterSeekTargetChange_StillStopsPlayback)
{
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Running));

    EXPECT_CALL(*m_player, setPlaybackRegion(PlaybackRegion { 3.0, 7.0 }))
    .Times(1);
    EXPECT_CALL(*m_player, stop())
    .Times(1);
    EXPECT_CALL(*m_player, seek(secs_t(3.0), false))
    .Times(1);
    EXPECT_CALL(*m_player, pause())
    .Times(0);

    m_transport->setLastPlaybackSeekTime(12.0);
    changePlaybackRegion(3.0, 7.0);
    pause();
}

/**
 * @brief Toggle play when already playing with run from start position
 * @details User clicked play with Shift modifier
 *          Playback should run from start position
 */
TEST_F(TransportTests, TogglePlay_WhenPlaying_PlayAgain)
{
    //! [GIVEN] Playback is running
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Running));

    //! [GIVEN] Play with Shift modifier
    ON_CALL(*m_application, keyboardModifiers())
    .WillByDefault(Return(Qt::ShiftModifier));

    //! [THEN] Player should stop playing
    EXPECT_CALL(*m_player, stop())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Toggle play when paused
 * @details User clicked play again for resume
 *          Playback should resume
 */
TEST_F(TransportTests, TogglePlay_WhenPaused)
{
    //! [GIVEN] Playback is paused
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Paused));

    //! [THEN] Player should resume playing
    EXPECT_CALL(*m_player, resume())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Toggle play when paused with skiping selection
 * @details User clicked play with Shift modifier
 *          Playback should resume from current position with ignoring selection
 */
TEST_F(TransportTests, TogglePlay_WhenPaused_WithIgnoreSelection)
{
    //! [GIVEN] Playback is paused
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Paused));

    //! [GIVEN] Play with Shift modifier
    ON_CALL(*m_application, keyboardModifiers())
    .WillByDefault(Return(Qt::ShiftModifier));

    //! [THEN] Expect that playbeck should run from current position
    secs_t currentPosition = 10.0;
    EXPECT_CALL(*m_player, playbackPosition())
    .WillRepeatedly(Return(currentPosition));
    EXPECT_CALL(*m_player, seek(currentPosition, false))
    .WillRepeatedly(Return());

    //! [THEN] No checking selection
    EXPECT_CALL(*m_selectionController, timeSelectionIsEmpty())
    .Times(0);

    //! [THEN] Player should start playing
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Toggle play when paused with changing selection
 * @details User clicked play after changing selection region
 *          Playback should run from selection start position
 */
TEST_F(TransportTests, TogglePlay_WhenPaused_WithChangingSelection)
{
    //! [GIVEN] Play with Shift modifier
    EXPECT_CALL(*m_application, keyboardModifiers())
    .WillOnce(Return(Qt::ShiftModifier))
    .WillRepeatedly(Return(Qt::NoModifier));

    //! [GIVEN] User started playback
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));
    togglePlay();

    //! [GIVEN] And paused it
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Running));
    togglePlay();

    //! [GIVEN] In paused state
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Paused));

    //! [THEN] Expect that playbeck should run from selection start position
    PlaybackRegion selectionRegion = { secs_t(10.0), secs_t(20.0) };
    EXPECT_CALL(*m_selectionController, timeSelectionIsEmpty())
    .WillOnce(Return(false));
    EXPECT_CALL(*m_selectionController, dataSelectedStartTime())
    .WillOnce(Return(selectionRegion.start));
    EXPECT_CALL(*m_selectionController, dataSelectedEndTime())
    .WillOnce(Return(selectionRegion.end));

    //! [THEN] Expect that we will take into account the selection region
    EXPECT_CALL(*m_player, setPlaybackRegion(selectionRegion))
    .WillRepeatedly(Return());

    //! [THEN] Player should stop playing
    EXPECT_CALL(*m_player, stop())
    .Times(1);

    //! [THEN] Player should start playing
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] Fitst: user changed selection
    changePlaybackRegion(selectionRegion.start, selectionRegion.end);

    //! [WHEN] Second: toggle play
    togglePlay();
}

/**
 * @brief Toggle play when there is selection wich start time is more than total time
 * @details User made a selection and clicked play
 *          Playback shouldn't be started
 */
TEST_F(TransportTests, TogglePlay_WithSelection_StartTimeIsMoreThanTotalTime)
{
    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [GIVEN] There is selection from 10 to 20 secs
    PlaybackRegion selectionRegion = { secs_t(1000.0), secs_t(2000.0) };
    EXPECT_CALL(*m_selectionController, timeSelectionIsEmpty())
    .WillOnce(Return(false));
    EXPECT_CALL(*m_selectionController, dataSelectedStartTime())
    .WillOnce(Return(selectionRegion.start));
    EXPECT_CALL(*m_selectionController, dataSelectedEndTime())
    .WillOnce(Return(selectionRegion.end));

    //! [THEN] Expect that we will take into account the selection region
    EXPECT_CALL(*m_player, setPlaybackRegion(selectionRegion))
    .Times(1);

    //! [THEN] Player should start playing
    EXPECT_CALL(*m_player, play())
    .Times(0);

    //! [WHEN] Toggle play
    togglePlay();
}

/**
 * @brief Seek playback position to a new time
 * @details User clicked on the clips view
 *          Player should only seek to new time
 */
TEST_F(TransportTests, Seek_WhenNotPlaying)
{
    //! [GIVEN] New seek time
    secs_t newSeekTime = 10.0;

    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [THEN] Playback will be seek to the new seek position
    EXPECT_CALL(*m_player, seek(newSeekTime, false /* applyIfPlaying */))
    .Times(1);

    //! [WHEN] Seek to the new time
    seek(newSeekTime);
}

/**
 * @brief Seek playback position to a new time when paused
 * @details User clicked on the clips view
 *          Player should stop and seek to new time
 */
TEST_F(TransportTests, Seek_WhenPaused)
{
    //! [GIVEN] New seek time
    secs_t newSeekTime = 10.0;

    //! [GIVEN] Playback is paused
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Paused));

    //! [THEN] Playback will be seek to the new seek position
    EXPECT_CALL(*m_player, seek(newSeekTime, false /* applyIfPlaying */))
    .Times(1);

    //! [THEN] Player should stop playing
    EXPECT_CALL(*m_player, stop())
    .Times(1);

    //! [WHEN] Seek to the new time
    seek(newSeekTime);
}

/**
 * @brief Seek playback position to a new time with triggering play
 * @details User clicked on the bottom section of timeline
 *          Player should seek to new time and start playing
 */
TEST_F(TransportTests, Seek_WithTriggeringPlay)
{
    //! [GIVEN] New seek time
    secs_t newSeekTime = 10.0;

    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [THEN] Playback will be seek to the new seek position
    EXPECT_CALL(*m_player, seek(newSeekTime, true /* applyIfPlaying */))
    .Times(1);

    //! [THEN] Player should start playing
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] Seek to the new time with triggering play
    seek(newSeekTime, true);
}

/**
 * @brief Seek playback position to a new time with triggering play and playback is already playing
 * @details User clicked on the bottom section of timeline
 *          Player should only seek to new time
 */
TEST_F(TransportTests, Seek_WithTriggeringPlay_AlreadyPlaying)
{
    //! [GIVEN] New seek time
    secs_t newSeekTime = 10.0;

    //! [GIVEN] Playback is running
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Running));

    //! [THEN] Playback will be seek to the new seek position
    EXPECT_CALL(*m_player, seek(newSeekTime, true /* applyIfPlaying */))
    .Times(1);

    //! [THEN] Player shouldn't start playing again
    EXPECT_CALL(*m_player, play())
    .Times(0);

    //! [WHEN] Seek to the new time with triggering play
    seek(newSeekTime, true);
}

/**
 * @brief Seek playback position to a new time that is more than total time with triggering play
 * @details User clicked on the bottom section of timeline
 *          Player should only seek to new time without play
 */
TEST_F(TransportTests, Seek_WithTriggeringPlay_FromTimeThatIsMoreThanTotalTime)
{
    //! [GIVEN] New seek time more than total time
    secs_t newSeekTime = 1000.0;

    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [THEN] Playback will be seek to the new seek position
    EXPECT_CALL(*m_player, seek(newSeekTime, true /* applyIfPlaying */))
    .Times(1);

    //! [THEN] Player shouldn't start playing
    EXPECT_CALL(*m_player, play())
    .Times(0);

    //! [WHEN] Seek to the new time with triggering play
    seek(newSeekTime, true);
}

/**
 * @brief Rewind to start
 * @details User clicked rewind to start button
 *         Selection should be cleared
 */
TEST_F(TransportTests, Rewind_ToStart_CheckSelectionReset)
{
    //! [GIVEN] No matter of current clip/range selection

    //! [THEN]
    //! Time (clip or range) selection is reset
    EXPECT_CALL(*m_selectionController, resetTimeSelection())
    .Times(1);

    //! [WHEN] Rewind to start
    rewindToStart();
}

/**
 * @brief Rewind to end
 * @details User clicked rewind to end button
 *          Selection should be cleared
 */
TEST_F(TransportTests, Rewind_ToEnd_CheckSelectionReset)
{
    //! [GIVEN] No matter of current clip/range selection

    //! [THEN]
    //! Time (clip or range) selection is reset
    EXPECT_CALL(*m_selectionController, resetTimeSelection())
    .Times(1);

    //! [WHEN] Rewind to end
    rewindToEnd();
}

/**
 * @brief Seek then stopSeekAndUpdatePlaybackRegion should keep the cursor.
 * @details User clicks the cursor at 42s, then triggers a stop-and-update
 *          (e.g. via Shift+Space while playing). The playback region forwarded
 *          to the player should be the cursor, not an empty region.
 */
TEST_F(TransportTests, StopSeekAndUpdatePlaybackRegion_PreservesSeekPosition)
{
    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [GIVEN] No active playback region (seek-validity check uses totalPlayTime)
    ON_CALL(*m_player, playbackRegion())
    .WillByDefault(Return(PlaybackRegion {}));

    const secs_t cursor = 42.0;

    //! [THEN] Player is seeked to the cursor (once by the click, once by
    //! the subsequent stop-and-update)
    EXPECT_CALL(*m_player, seek(cursor, false))
    .Times(2);

    //! [THEN] stopSeekAndUpdatePlaybackRegion stops the player
    EXPECT_CALL(*m_player, stop())
    .Times(1);

    //! [THEN] The playback region forwarded to the player is the cursor
    EXPECT_CALL(*m_player, setPlaybackRegion(PlaybackRegion { cursor, cursor }))
    .Times(1);

    //! [WHEN] User clicks the cursor at 42s
    seek(cursor, false);

    //! [WHEN] Then triggers a stop-and-update
    m_transport->stopSeekAndUpdatePlaybackRegion();
}

/**
 * @brief Toggle play with no selection plays from the cursor to project end.
 * @details Cursor is at 30s (e.g. just after recording finished), nothing
 *          is selected. Pressing Space should set the playback region to
 *          {cursor, totalPlayTime} and start playing.
 */
TEST_F(TransportTests, TogglePlay_AfterRecord_PlaysFromSeekToProjectEnd)
{
    //! [GIVEN] Playback is stopped
    ON_CALL(*m_player, playbackStatus())
    .WillByDefault(Return(PlaybackStatus::Stopped));

    //! [GIVEN] Cursor is at 30s
    const secs_t recordEnd = 30.0;
    m_transport->setLastPlaybackSeekTime(recordEnd);

    //! [GIVEN] Playhead is at the cursor (not at project end)
    EXPECT_CALL(*m_player, playbackPosition())
    .WillRepeatedly(Return(recordEnd));

    //! [GIVEN] No selection
    EXPECT_CALL(*m_selectionController, leftMostSelectedItemStartTime())
    .WillOnce(Return(std::nullopt));
    EXPECT_CALL(*m_selectionController, rightMostSelectedItemEndTime())
    .WillOnce(Return(std::nullopt));
    EXPECT_CALL(*m_selectionController, timeSelectionIsEmpty())
    .WillOnce(Return(true));

    //! [THEN] Playback region is {cursor, totalPlayTime}
    EXPECT_CALL(*m_player, setPlaybackRegion(PlaybackRegion { recordEnd, secs_t(100.0) }))
    .Times(1);

    //! [THEN] Player starts playing
    EXPECT_CALL(*m_player, play())
    .Times(1);

    //! [WHEN] User presses Space
    togglePlay();
}

/**
 * @brief Changing the device while playing stops, applies the change, then resumes.
 * @details The device can only be switched while no stream is open (issue #11098).
 *          withStreamRestart() must own that: stop playback, apply the change,
 *          and resume playing from the same position — in that order.
 */
TEST_F(TransportTests, ChangeAudioDevice_WhilePlaying_StopsAppliesResumes)
{
    //! [GIVEN] Playback is running at 30s
    setupStatefulTransport(PlaybackStatus::Running);
    ON_CALL(*m_player, playbackPosition())
    .WillByDefault(Return(secs_t(30.0)));

    //! [THEN] The stream is stopped, the change is applied, then playback resumes;
    //! the playing state is not turned into a pause.
    EXPECT_CALL(*m_player, stop()).Times(1);
    EXPECT_CALL(*m_player, play()).Times(1);
    EXPECT_CALL(*m_player, pause()).Times(0);

    //! [WHEN] The device is changed
    withStreamRestart([this]() { m_events.push_back("apply"); });

    //! [THEN] The change happened after the stop and before the resume
    EXPECT_EQ(m_events, (std::vector<std::string> { "stop", "apply", "play" }));
}

/**
 * @brief Changing the device while paused stops and stays stopped.
 * @details The stream must be torn down for the switch; the transport is then
 *          left stopped — it is not resumed and not put back into pause.
 */
TEST_F(TransportTests, ChangeAudioDevice_WhilePaused_StopsWithoutResume)
{
    //! [GIVEN] Playback is paused at 30s
    setupStatefulTransport(PlaybackStatus::Paused);
    ON_CALL(*m_player, playbackPosition())
    .WillByDefault(Return(secs_t(30.0)));

    //! [THEN] The stream is stopped, the change is applied, and nothing resumes
    EXPECT_CALL(*m_player, stop()).Times(1);
    EXPECT_CALL(*m_player, play()).Times(0);
    EXPECT_CALL(*m_player, pause()).Times(0);

    //! [WHEN] The device is changed
    withStreamRestart([this]() { m_events.push_back("apply"); });

    //! [THEN] Only the stop and the change happened; transport stays stopped
    EXPECT_EQ(m_events, (std::vector<std::string> { "stop", "apply" }));
}

/**
 * @brief Changing the device while stopped only applies the change.
 * @details Nothing is playing, so the transport must not be touched.
 */
TEST_F(TransportTests, ChangeAudioDevice_WhileStopped_OnlyApplies)
{
    //! [GIVEN] Playback is stopped
    setupStatefulTransport(PlaybackStatus::Stopped);

    //! [THEN] The transport is left alone
    EXPECT_CALL(*m_player, stop()).Times(0);
    EXPECT_CALL(*m_player, play()).Times(0);
    EXPECT_CALL(*m_player, pause()).Times(0);

    //! [WHEN] The device is changed
    withStreamRestart([this]() { m_events.push_back("apply"); });

    //! [THEN] Only the change itself happened
    EXPECT_EQ(m_events, (std::vector<std::string> { "apply" }));
}

/**
 * @brief Changing the device while recording stops but does not auto-resume.
 * @details A capture stream cannot be resumed from a position, so recording is
 *          stopped (so the low-level switch never sees an open stream) but not
 *          restarted.
 */
TEST_F(TransportTests, ChangeAudioDevice_WhileRecording_StopsWithoutResume)
{
    //! [GIVEN] A recording is in progress
    setupStatefulTransport(PlaybackStatus::Stopped);
    EXPECT_CALL(*m_recordController, isRecording())
    .WillRepeatedly(Return(true));

    //! [THEN] The stream is stopped but playback is not resumed
    EXPECT_CALL(*m_player, stop()).Times(1);
    EXPECT_CALL(*m_player, play()).Times(0);
    EXPECT_CALL(*m_player, pause()).Times(0);

    //! [WHEN] The device is changed
    withStreamRestart([this]() { m_events.push_back("apply"); });

    //! [THEN] The stream was stopped before the change, with no resume afterwards
    EXPECT_EQ(m_events, (std::vector<std::string> { "stop", "apply" }));
}
}
