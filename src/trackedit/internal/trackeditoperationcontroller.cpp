/*
 * Audacity: A Digital Audio Editor
 */
#include "trackeditoperationcontroller.h"
#include "trackediterrors.h"

namespace au::trackedit {
TrackeditOperationController::TrackeditOperationController(std::unique_ptr<IUndoManager> undoManager)
    : m_undoManager{std::move(undoManager)} {}

secs_t TrackeditOperationController::clipStartTime(const ClipKey& clipKey) const
{
    return trackAndClipOperations()->clipStartTime(clipKey);
}

secs_t TrackeditOperationController::clipEndTime(const ClipKey& clipKey) const
{
    return trackAndClipOperations()->clipEndTime(clipKey);
}

bool TrackeditOperationController::changeClipStartTime(const ClipKey& clipKey, secs_t newStartTime, bool completed)
{
    return trackAndClipOperations()->changeClipStartTime(clipKey, newStartTime, completed);
}

muse::async::Channel<ClipKey, secs_t /*newStartTime*/, bool /*completed*/> TrackeditOperationController::clipStartTimeChanged() const
{
    return trackAndClipOperations()->clipStartTimeChanged();
}

bool TrackeditOperationController::trimTracksData(const std::vector<trackedit::TrackId>& tracksIds, secs_t begin, secs_t end)
{
    if (trackAndClipOperations()->trimTracksData(tracksIds, begin, end)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Trim selected audio tracks from %1 seconds to %2 seconds")
            .arg(begin).arg(end),
            muse::TranslatableString("trackedit", "Trim Audio")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::silenceTracksData(const std::vector<trackedit::TrackId>& tracksIds, secs_t begin, secs_t end)
{
    if (trackAndClipOperations()->silenceTracksData(tracksIds, begin, end)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Silenced selected tracks for %1 seconds at %2 seconds")
            .arg(begin).arg(end),
            muse::TranslatableString("trackedit", "Silence")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::changeTrackTitle(const trackedit::TrackId trackId, const muse::String& title)
{
    if (trackAndClipOperations()->changeTrackTitle(trackId, title)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Track Title"),
            muse::TranslatableString("trackedit", "Changed Track Title")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::changeClipTitle(const ClipKey& clipKey, const muse::String& newTitle)
{
    if (trackAndClipOperations()->changeClipTitle(clipKey, newTitle)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Clip Title"),
            muse::TranslatableString("trackedit", "Changed Clip Title")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::changeClipPitch(const ClipKey& clipKey, int pitch)
{
    if (trackAndClipOperations()->changeClipPitch(clipKey, pitch)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Pitch Shift"),
            muse::TranslatableString("trackedit", "Changed Pitch Shift")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::resetClipPitch(const ClipKey& clipKey)
{
    if (trackAndClipOperations()->resetClipPitch(clipKey)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Reset Clip Pitch"),
            muse::TranslatableString("trackedit", "Reset Clip Pitch")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::changeClipSpeed(const ClipKey& clipKey, double speed)
{
    if (trackAndClipOperations()->changeClipSpeed(clipKey, speed)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Changed Speed"),
            muse::TranslatableString("trackedit", "Changed Speed")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::resetClipSpeed(const ClipKey& clipKey)
{
    if (trackAndClipOperations()->resetClipSpeed(clipKey)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Reset Clip Speed"),
            muse::TranslatableString("trackedit", "Reset Clip Speed")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::changeClipColor(const ClipKey& clipKey, const std::string& color)
{
    if (trackAndClipOperations()->changeClipColor(clipKey, color)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Changed Clip Color"),
            muse::TranslatableString("trackedit", "Change Clip Color")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::changeTracksColor(const TrackIdList& tracksIds, const std::string& color)
{
    if (trackAndClipOperations()->changeTracksColor(tracksIds, color)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Changed track color"),
            muse::TranslatableString("trackedit", "Change track color")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::changeClipOptimizeForVoice(const ClipKey& clipKey, bool optimize)
{
    return trackAndClipOperations()->changeClipOptimizeForVoice(clipKey, optimize);
}

bool TrackeditOperationController::renderClipPitchAndSpeed(const ClipKey& clipKey)
{
    if (trackAndClipOperations()->renderClipPitchAndSpeed(clipKey)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Rendered time-stretched audio"),
            muse::TranslatableString("trackedit", "Render")
            );
        return true;
    }
    return false;
}

void TrackeditOperationController::clearClipboard()
{
    clipboard()->clearTrackData();
}

muse::Ret TrackeditOperationController::pasteFromClipboard(secs_t begin, bool moveClips, bool moveAllTracks)
{
    auto modifiedState = false;
    const auto ret = trackAndClipOperations()->paste(clipboard()->trackDataCopy(), begin, moveClips, moveAllTracks,
                                                     clipboard()->isMultiSelectionCopy(), modifiedState);
    if (ret) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Pasted from the clipboard"),
            muse::TranslatableString("trackedit", "Paste")
            );
    } else if (modifiedState) {
        projectHistory()->rollbackState();
        globalContext()->currentTrackeditProject()->reload();
    }
    return ret;
}

bool TrackeditOperationController::cutClipIntoClipboard(const ClipKey& clipKey)
{
    ITrackDataPtr data = trackAndClipOperations()->cutClip(clipKey);
    if (!data) {
        return false;
    }
    clipboard()->addTrackData(std::move(data));
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Cut to the clipboard"),
        muse::TranslatableString("trackedit", "Cut")
        );
    return true;
}

bool TrackeditOperationController::cutClipDataIntoClipboard(const TrackIdList& tracksIds, secs_t begin, secs_t end, bool moveClips)
{
    std::vector<ITrackDataPtr> tracksData(tracksIds.size());
    for (const auto& trackId : tracksIds) {
        const auto data = trackAndClipOperations()->cutTrackData(trackId, begin, end, moveClips);
        if (!data) {
            return false;
        }
        tracksData.push_back(std::move(data));
    }
    for (auto& trackData : tracksData) {
        clipboard()->addTrackData(std::move(trackData));
    }
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Cut to the clipboard"),
        muse::TranslatableString("trackedit", "Cut")
        );
    return true;
}

bool TrackeditOperationController::copyClipIntoClipboard(const ClipKey& clipKey)
{
    ITrackDataPtr data = trackAndClipOperations()->copyClip(clipKey);
    if (!data) {
        return false;
    }
    clipboard()->addTrackData(std::move(data));
    return true;
}

bool TrackeditOperationController::copyNonContinuousTrackDataIntoClipboard(const TrackId trackId, const ClipKeyList& clipKeys,
                                                                           secs_t offset)
{
    ITrackDataPtr data = trackAndClipOperations()->copyNonContinuousTrackData(trackId, clipKeys, offset);
    if (!data) {
        return false;
    }
    clipboard()->addTrackData(std::move(data));
    if (clipKeys.size() > 1) {
        clipboard()->setMultiSelectionCopy(true);
    }
    return true;
}

bool TrackeditOperationController::copyContinuousTrackDataIntoClipboard(const TrackId trackId, secs_t begin, secs_t end)
{
    ITrackDataPtr data = trackAndClipOperations()->copyContinuousTrackData(trackId, begin, end);
    if (!data) {
        return false;
    }
    clipboard()->addTrackData(std::move(data));
    return true;
}

bool TrackeditOperationController::removeClip(const ClipKey& clipKey)
{
    if (const std::optional<TimeSpan> span = trackAndClipOperations()->removeClip(clipKey)) {
        pushProjectHistoryDeleteState(span->start(), span->duration());
        return true;
    }
    return false;
}

bool TrackeditOperationController::removeClips(const ClipKeyList& clipKeyList, bool moveClips)
{
    if (trackAndClipOperations()->removeClips(clipKeyList, moveClips)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Delete"),
            muse::TranslatableString("trackedit", "Delete multiple clips")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::removeTracksData(const TrackIdList& tracksIds, secs_t begin, secs_t end, bool moveClips)
{
    if (trackAndClipOperations()->removeTracksData(tracksIds, begin, end, moveClips)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Delete"),
            muse::TranslatableString("trackedit", "Delete and close gap")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::moveClips(secs_t timePositionOffset, int trackPositionOffset, bool completed,
                                             bool& clipsMovedToOtherTrack)
{
    auto success = true;
    if (!trackAndClipOperations()->moveClips(timePositionOffset, trackPositionOffset, completed, clipsMovedToOtherTrack)) {
        success = false;
        if (completed) {
            clipsMovedToOtherTrack = false;
            projectHistory()->rollbackState();
            globalContext()->currentTrackeditProject()->reload();
        }
    } else if (completed) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Clip moved"),
            muse::TranslatableString("trackedit", "Move clip")
            );
    }
    return success;
}

bool TrackeditOperationController::splitTracksAt(const TrackIdList& tracksIds, std::vector<secs_t> pivots)
{
    if (trackAndClipOperations()->splitTracksAt(tracksIds, pivots)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Split"),
            muse::TranslatableString("trackedit", "Split")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::splitClipsAtSilences(const ClipKeyList& clipKeyList)
{
    if (trackAndClipOperations()->splitClipsAtSilences(clipKeyList)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Split clips at silence"),
            muse::TranslatableString("trackedit", "Split at silence")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::splitRangeSelectionAtSilences(const TrackIdList& tracksIds, secs_t begin, secs_t end)
{
    if (trackAndClipOperations()->splitRangeSelectionAtSilences(tracksIds, begin, end)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Split clips at silence"),
            muse::TranslatableString("trackedit", "Split at silence")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::splitRangeSelectionIntoNewTracks(const TrackIdList& tracksIds, secs_t begin, secs_t end)
{
    if (trackAndClipOperations()->splitRangeSelectionIntoNewTracks(tracksIds, begin, end)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Split into new track"),
            muse::TranslatableString("trackedit", "Split into new track")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::splitClipsIntoNewTracks(const ClipKeyList& clipKeyList)
{
    if (trackAndClipOperations()->splitClipsIntoNewTracks(clipKeyList)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Split into new track"),
            muse::TranslatableString("trackedit", "Split into new track")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::mergeSelectedOnTracks(const TrackIdList& tracksIds, secs_t begin, secs_t end)
{
    if (trackAndClipOperations()->mergeSelectedOnTracks(tracksIds, begin, end)) {
        const secs_t duration = end - begin;
        pushProjectHistoryJoinState(begin, duration);
        return true;
    }
    return false;
}

bool TrackeditOperationController::duplicateSelectedOnTracks(const TrackIdList& tracksIds, secs_t begin, secs_t end)
{
    if (trackAndClipOperations()->duplicateSelectedOnTracks(tracksIds, begin, end)) {
        pushProjectHistoryDuplicateState();
        return true;
    }
    return false;
}

bool TrackeditOperationController::duplicateClip(const ClipKey& clipKey)
{
    return trackAndClipOperations()->duplicateClip(clipKey);
}

bool TrackeditOperationController::duplicateClips(const ClipKeyList& clipKeyList)
{
    if (trackAndClipOperations()->duplicateClips(clipKeyList)) {
        pushProjectHistoryDuplicateState();
        return true;
    }
    return false;
}

bool TrackeditOperationController::clipSplitCut(const ClipKey& clipKey)
{
    ITrackDataPtr data = trackAndClipOperations()->clipSplitCut(clipKey);
    if (!data) {
        return false;
    }
    clipboard()->addTrackData(std::move(data));
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Split-cut to the clipboard"),
        muse::TranslatableString("trackedit", "Split cut")
        );
    return true;
}

bool TrackeditOperationController::clipSplitDelete(const ClipKey& clipKey)
{
    if (trackAndClipOperations()->clipSplitDelete(clipKey)) {
        pushProjectHistorySplitDeleteState();
        return true;
    }
    return false;
}

bool TrackeditOperationController::splitCutSelectedOnTracks(const TrackIdList tracksIds, secs_t begin, secs_t end)
{
    std::vector<ITrackDataPtr> tracksData = trackAndClipOperations()->splitCutSelectedOnTracks(tracksIds, begin, end);
    if (tracksData.empty()) {
        return false;
    }
    for (auto& trackData : tracksData) {
        clipboard()->addTrackData(std::move(trackData));
    }
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Split-cut to the clipboard"),
        muse::TranslatableString("trackedit", "Split cut")
        );
    return true;
}

bool TrackeditOperationController::splitDeleteSelectedOnTracks(const TrackIdList tracksIds, secs_t begin, secs_t end)
{
    if (trackAndClipOperations()->splitDeleteSelectedOnTracks(tracksIds, begin, end)) {
        pushProjectHistorySplitDeleteState();
        return true;
    }
    return false;
}

bool TrackeditOperationController::trimClipLeft(const ClipKey& clipKey, secs_t deltaSec, secs_t minClipDuration, bool completed,
                                                UndoPushType type)
{
    const auto success = trackAndClipOperations()->trimClipLeft(clipKey, deltaSec, minClipDuration, completed);
    if (success && completed) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Clip left trimmed"),
            muse::TranslatableString("trackedit", "Trim clip left"),
            type
            );
    }
    return success;
}

bool TrackeditOperationController::trimClipRight(const ClipKey& clipKey, secs_t deltaSec, secs_t minClipDuration, bool completed,
                                                 UndoPushType type)
{
    const auto success = trackAndClipOperations()->trimClipRight(clipKey, deltaSec, minClipDuration, completed);
    if (success && completed) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Clip right trimmed"),
            muse::TranslatableString("trackedit", "Trim clip right"),
            type
            );
    }
    return success;
}

bool TrackeditOperationController::stretchClipLeft(const ClipKey& clipKey, secs_t deltaSec, secs_t minClipDuration, bool completed,
                                                   UndoPushType type)
{
    const auto success = trackAndClipOperations()->stretchClipLeft(clipKey, deltaSec, minClipDuration, completed);
    if (success && completed) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Clip left stretched"),
            muse::TranslatableString("trackedit", "Stretch clip left"),
            type
            );
    }
    return success;
}

bool TrackeditOperationController::stretchClipRight(const ClipKey& clipKey, secs_t deltaSec, secs_t minClipDuration, bool completed,
                                                    UndoPushType type)
{
    const auto success = trackAndClipOperations()->stretchClipRight(clipKey, deltaSec, minClipDuration, completed);
    if (success && completed) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Clip right stretched"),
            muse::TranslatableString("trackedit", "Stretch clip right"),
            type
            );
    }
    return success;
}

secs_t TrackeditOperationController::clipDuration(const ClipKey& clipKey) const
{
    return trackAndClipOperations()->clipDuration(clipKey);
}

std::optional<secs_t> TrackeditOperationController::getLeftmostClipStartTime(const ClipKeyList& clipKeys) const
{
    return trackAndClipOperations()->getLeftmostClipStartTime(clipKeys);
}

bool TrackeditOperationController::newMonoTrack()
{
    if (trackAndClipOperations()->newMonoTrack()) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Created new audio track"),
            muse::TranslatableString("trackedit", "New track")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::newStereoTrack()
{
    if (trackAndClipOperations()->newStereoTrack()) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Created new audio track"),
            muse::TranslatableString("trackedit", "New track")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::newLabelTrack()
{
    return trackAndClipOperations()->newLabelTrack();
}

bool TrackeditOperationController::deleteTracks(const TrackIdList& trackIds)
{
    if (trackAndClipOperations()->deleteTracks(trackIds)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Delete track"),
            muse::TranslatableString("trackedit", "Delete track")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::duplicateTracks(const TrackIdList& trackIds)
{
    if (trackAndClipOperations()->duplicateTracks(trackIds)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Duplicate track"),
            muse::TranslatableString("trackedit", "Duplicate track")
            );
        return true;
    }
    return false;
}

void TrackeditOperationController::moveTracks(const TrackIdList& trackIds, TrackMoveDirection direction)
{
    if (trackAndClipOperations()->moveTracks(trackIds, direction)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Move track"),
            muse::TranslatableString("trackedit", "Move track")
            );
    }
}

void TrackeditOperationController::moveTracksTo(const TrackIdList& trackIds, int pos)
{
    if (trackAndClipOperations()->moveTracksTo(trackIds, pos)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Move track"),
            muse::TranslatableString("trackedit", "Move track")
            );
    }
}

bool TrackeditOperationController::undo()
{
    return m_undoManager->undo();
}

bool TrackeditOperationController::canUndo()
{
    return m_undoManager->canUndo();
}

bool TrackeditOperationController::redo()
{
    return m_undoManager->redo();
}

bool TrackeditOperationController::canRedo()
{
    return m_undoManager->canRedo();
}

bool TrackeditOperationController::undoRedoToIndex(size_t index)
{
    return m_undoManager->undoRedoToIndex(index);
}

bool TrackeditOperationController::insertSilence(const TrackIdList& trackIds, secs_t begin, secs_t end, secs_t duration)
{
    if (trackAndClipOperations()->insertSilence(trackIds, begin, end, duration)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Insert silence"),
            muse::TranslatableString("trackedit", "Insert silence")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::toggleStretchToMatchProjectTempo(const ClipKey& clipKey)
{
    return trackAndClipOperations()->toggleStretchToMatchProjectTempo(clipKey);
}

int64_t TrackeditOperationController::clipGroupId(const trackedit::ClipKey& clipKey) const
{
    return trackAndClipOperations()->clipGroupId(clipKey);
}

void TrackeditOperationController::setClipGroupId(const trackedit::ClipKey& clipKey, int64_t id)
{
    trackAndClipOperations()->setClipGroupId(clipKey, id);
}

void TrackeditOperationController::groupClips(const trackedit::ClipKeyList& clipKeyList)
{
    trackAndClipOperations()->groupClips(clipKeyList);
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Clips grouped"),
        muse::TranslatableString("trackedit", "Clips grouped")
        );
}

void TrackeditOperationController::ungroupClips(const trackedit::ClipKeyList& clipKeyList)
{
    trackAndClipOperations()->ungroupClips(clipKeyList);
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Clips ungrouped"),
        muse::TranslatableString("trackedit", "Clips ungrouped")
        );
}

ClipKeyList TrackeditOperationController::clipsInGroup(int64_t id) const
{
    return trackAndClipOperations()->clipsInGroup(id);
}

bool TrackeditOperationController::changeTracksFormat(const TrackIdList& tracksIds, trackedit::TrackFormat format)
{
    if (trackAndClipOperations()->changeTracksFormat(tracksIds, format)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Changed track format"),
            muse::TranslatableString("trackedit", "Changed track format")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::changeTracksRate(const TrackIdList& tracksIds, int rate)
{
    if (trackAndClipOperations()->changeTracksRate(tracksIds, rate)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Changed track rate"),
            muse::TranslatableString("trackedit", "Changed track rate")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::swapStereoChannels(const TrackIdList& tracksIds)
{
    if (trackAndClipOperations()->swapStereoChannels(tracksIds)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Swapped stereo channels"),
            muse::TranslatableString("trackedit", "Swapped stereo channels")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::splitStereoTracksToLRMono(const TrackIdList& tracksIds)
{
    if (trackAndClipOperations()->splitStereoTracksToLRMono(tracksIds)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Split stereo tracks to L/R mono"),
            muse::TranslatableString("trackedit", "Split stereo tracks to L/R mono")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::splitStereoTracksToCenterMono(const TrackIdList& tracksIds)
{
    if (trackAndClipOperations()->splitStereoTracksToCenterMono(tracksIds)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Split stereo tracks to center mono"),
            muse::TranslatableString("trackedit", "Split stereo tracks to center mono")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::makeStereoTrack(const TrackId left, const TrackId right)
{
    if (trackAndClipOperations()->makeStereoTrack(left, right)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Make stereo track"),
            muse::TranslatableString("trackedit", "Make stereo track")
            );
        return true;
    }
    return false;
}

bool TrackeditOperationController::resampleTracks(const TrackIdList& tracksIds, int rate)
{
    if (trackAndClipOperations()->resampleTracks(tracksIds, rate)) {
        projectHistory()->pushHistoryState(
            muse::TranslatableString("trackedit", "Resampled audio track(s) to %1 Hz").arg(rate),
            muse::TranslatableString("trackedit", "Resample track")
            );
        return true;
    }
    return false;
}

muse::ProgressPtr TrackeditOperationController::progress() const
{
    return trackAndClipOperations()->progress();
}

void TrackeditOperationController::pushProjectHistoryJoinState(secs_t start, secs_t duration)
{
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Joined %1 seconds at %2")
        .arg(duration).arg(start),
        muse::TranslatableString("trackedit", "Join")
        );
}

void TrackeditOperationController::pushProjectHistoryDuplicateState()
{
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Duplicated"),
        muse::TranslatableString("trackedit", "Duplicate")
        );
}

void TrackeditOperationController::pushProjectHistorySplitDeleteState()
{
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Split-deleted clips"),
        muse::TranslatableString("trackedit", "Split delete")
        );
}

void TrackeditOperationController::pushProjectHistoryDeleteState(secs_t start, secs_t duration)
{
    projectHistory()->pushHistoryState(
        muse::TranslatableString("trackedit", "Delete %1 seconds at %2")
        .arg(duration).arg(start),
        muse::TranslatableString("trackedit", "Delete")
        );
}
}
