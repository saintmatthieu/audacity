/*
* Audacity: A Digital Audio Editor
*/
#include "au3project.h"

#include "global/defer.h"

#include "libraries/lib-project/Project.h"
#include "libraries/lib-project-file-io/ProjectFileIO.h"
#include "libraries/lib-wave-track/WaveTrack.h"
#include "libraries/lib-wave-track/WaveClip.h"
#include "libraries/lib-numeric-formats/ProjectTimeSignature.h"
#include "libraries/lib-project-history/ProjectHistory.h"
#include "libraries/lib-realtime-effects/SavedMasterEffectList.h"
#include "domconverter.h"
#include "TempoChange.h"

//! HACK
//! Static variable is not initialized
//! static SampleBlockFactory::Factory::Scope scope{ []( AudacityProject &project )
//! so, to fix it, included this file here
#include "libraries/lib-project-file-io/SqliteSampleBlock.cpp"

#include "wxtypes_convert.h"
#include "../au3types.h"

#include "log.h"

using namespace au::au3;

std::shared_ptr<IAu3Project> Au3ProjectCreator::create() const
{
    return std::make_shared<Au3ProjectAccessor>();
}

struct au::au3::Au3ProjectData
{
    std::shared_ptr<Au3Project> project;

    Au3Project& projectRef() { return *project.get(); }
};

Au3ProjectAccessor::Au3ProjectAccessor()
    : m_data(std::make_shared<Au3ProjectData>())
{
    m_data->project = Au3Project::Create();
    mTrackListSubstription = Au3TrackList::Get(m_data->projectRef()).Subscribe([this](const TrackListEvent& event)
    {
        if (event.mType == TrackListEvent::ADDITION) {
            const auto tempo = ProjectTimeSignature::Get(m_data->projectRef()).GetTempo();
            if (const auto track = event.mpTrack.lock()) {
                DoProjectTempoChange(*track, tempo);
            }
        }
    });
}

void Au3ProjectAccessor::open()
{
    auto& projectFileIO = ProjectFileIO::Get(m_data->projectRef());
    projectFileIO.OpenProject();
}

bool Au3ProjectAccessor::load(const muse::io::path_t& filePath)
{
    auto& projectFileIO = ProjectFileIO::Get(m_data->projectRef());
    std::string sstr = filePath.toStdString();
    FilePath fileName = wxString::FromUTF8(sstr.c_str(), sstr.size());

    std::optional<ProjectFileIO::TentativeConnection> conn;

    try {
        conn.emplace(projectFileIO.LoadProject(fileName, false /*ignoreAutosave*/).value());
    } catch (AudacityException&) {
        LOGE() << "failed load project: " << filePath << ", exception received";
        return false;
    }

    muse::Defer([&conn]() {
        conn->Commit();
    });

    const bool bParseSuccess = conn.has_value();
    if (!bParseSuccess) {
        LOGE() << "failed load project: " << filePath;
        return false;
    }

    //! TODO Look like, need doing all from method  ProjectFileManager::FixTracks
    //! and maybe what is done before this method (ProjectFileManager::ReadProjectFile)
    Au3TrackList& tracks = Au3TrackList::Get(m_data->projectRef());
    for (auto pTrack : tracks) {
        pTrack->LinkConsistencyFix();
    }

    mLastSavedTracks = TrackList::Create(nullptr);
    for (const WaveTrack* track : tracks.Any<WaveTrack>()) {
        mLastSavedTracks->Add(
            track->Duplicate(Track::DuplicateOptions {}.Backup()));
    }

    m_hasSavedVersion = true;

    return true;
}

bool Au3ProjectAccessor::save(const muse::io::path_t& filePath)
{
    SavedMasterEffectList::Get(m_data->projectRef()).OnProjectAboutToBeSaved();

    auto& projectFileIO = ProjectFileIO::Get(m_data->projectRef());
    auto result = projectFileIO.SaveProject(wxFromString(filePath.toString()), mLastSavedTracks.get());
    if (result) {
        UndoManager::Get(m_data->projectRef()).StateSaved();
        // Project is now saved on disk - this isn't a new project anymore.
        m_hasSavedVersion = true;
    }

    if (mLastSavedTracks) {
        mLastSavedTracks->Clear();
    }
    mLastSavedTracks = TrackList::Create(nullptr);
    auto& tracks = TrackList::Get(m_data->projectRef());
    for (auto t : tracks) {
        mLastSavedTracks->Add(t->Duplicate(Track::DuplicateOptions {}.Backup()));
    }

    return result;
}

void Au3ProjectAccessor::close()
{
    //! ============================================================================
    //! NOTE Step 1
    //! ============================================================================
    auto& project = m_data->projectRef();
    auto& undoManager = UndoManager::Get(project);
    if (undoManager.GetSavedState() >= 0) {
        constexpr auto doAutoSave = false;
        ProjectHistory::Get(project).SetStateTo(
            undoManager.GetSavedState(), doAutoSave);
    }

    //! ============================================================================
    //! NOTE Step 2 - We compact the project only around the tracks that were actually
    //! saved. This will allow us to later on clear the UndoManager states bypassing
    //! the deletion of blocks 1 by 1, which can take a long time for projects with
    //!  large unsaved audio ; see https://github.com/audacity/audacity/issues/7382
    //! ============================================================================
    auto& projectFileIO = ProjectFileIO::Get(project);

    // Lock all blocks in all tracks of the last saved version, so that
    // the sample blocks aren't deleted from the database when we destroy the
    // sample block objects in memory.
    if (mLastSavedTracks) {
        for (auto wt : mLastSavedTracks->Any<WaveTrack>()) {
            WaveTrackUtilities::CloseLock(*wt);
        }

        // Attempt to compact the project
        projectFileIO.Compact({ mLastSavedTracks.get() });

        if (
            !projectFileIO.WasCompacted() && undoManager.UnsavedChanges()) {
            // If compaction failed, we must do some work in case of close
            // without save.  Don't leave the document blob from the last
            // push of undo history, when that undo state may get purged
            // with deletion of some new sample blocks.
            // REVIEW: UpdateSaved() might fail too.  Do we need to test
            // for that and report it?
            projectFileIO.UpdateSaved(mLastSavedTracks.get());
        }
    }

    //! ============================================================================
    //! NOTE Step 3
    //! Set (or not) the bypass flag to indicate that deletes that would happen
    //! during undoManager.ClearStates() below are not necessary. Must be called
    //! between `CompactProjectOnClose()` and `undoManager.ClearStates()`.
    //! ============================================================================
    projectFileIO.SetBypass();

    // This can reduce reference counts of sample blocks in the project's
    // tracks.
    undoManager.ClearStates();

    // Delete all the tracks to free up memory
    TrackList::Get(project).Clear();

    projectFileIO.CloseProject();

    if (mLastSavedTracks) {
        mLastSavedTracks->Clear();
        mLastSavedTracks.reset();
    }
}

std::string Au3ProjectAccessor::title() const
{
    if (!m_data->project) {
        return std::string();
    }

    return wxToStdSting(m_data->project->GetProjectName());
}

bool Au3ProjectAccessor::hasSavedVersion() const
{
    return m_hasSavedVersion;
}

uintptr_t Au3ProjectAccessor::au3ProjectPtr() const
{
    return reinterpret_cast<uintptr_t>(m_data->project.get());
}
