#include "realtimeeffectservice.h"

#include "libraries/lib-audio-io/AudioIO.h"

#include "libraries/lib-wave-track/WaveTrack.h"
#include "libraries/lib-effects/EffectManager.h"
#include "libraries/lib-realtime-effects/RealtimeEffectState.h"
#include "libraries/lib-realtime-effects/RealtimeEffectList.h"
#include "libraries/lib-project-history/UndoManager.h"
#include "libraries/lib-project/Project.h"

#include "au3wrap/au3types.h"
#include "au3wrap/internal/domaccessor.h"

#include "project/iaudacityproject.h"

#include "global/types/translatablestring.h"

#include <unordered_map>

namespace au::effects {
namespace {
struct RealtimeEffectStackChanged : public ClientData::Base
{
    static RealtimeEffectStackChanged& Get(AudacityProject& project);

    muse::async::Channel<TrackId> channel;
};

static const AudacityProject::AttachedObjects::RegisteredFactory key{
    [](au3::Au3Project&)
    {
        return std::make_shared<RealtimeEffectStackChanged>();
    } };

RealtimeEffectStackChanged& RealtimeEffectStackChanged::Get(AudacityProject& project)
{
    return project.AttachedObjects::Get<RealtimeEffectStackChanged>(key);
}

auto trackEffectMap(au3::Au3Project& project)
{
    std::unordered_map<effects::TrackId, std::unique_ptr<RealtimeEffectList> > map;
    for (WaveTrack* track : TrackList::Get(project).Any<WaveTrack>()) {
        map.emplace(track->GetId(), RealtimeEffectList::Get(*track).Duplicate());
    }
    return map;
}

auto effectLists(au3::Au3Project& project)
{
    std::unordered_map<effects::TrackId, std::unique_ptr<RealtimeEffectList> > map;
    for (WaveTrack* track : TrackList::Get(project).Any<WaveTrack>()) {
        map.emplace(track->GetId(), RealtimeEffectList::Get(*track).Duplicate());
    }
    map.emplace(IRealtimeEffectService::masterTrackId, RealtimeEffectList::Get(project).Duplicate());
    return map;
}

bool operator!=(const RealtimeEffectList& a, const RealtimeEffectList& b)
{
    if (a.GetStatesCount() != b.GetStatesCount() || a.IsActive() != b.IsActive()) {
        return true;
    }
    for (auto i = 0; i < static_cast<int>(a.GetStatesCount()); ++i) {
        if (a.GetStateAt(i) != b.GetStateAt(i)) {
            return true;
        }
    }
    return false;
}

auto getStacks(au3::Au3Project& project)
{
    std::unordered_map<effects::TrackId, std::vector<RealtimeEffectStatePtr> > stacks;
    auto& masterList = RealtimeEffectList::Get(project);
    auto& masterStack = stacks[IRealtimeEffectService::masterTrackId];
    masterStack.reserve(masterList.GetStatesCount());
    for (auto i = 0; i < static_cast<int>(masterList.GetStatesCount()); ++i) {
        masterStack.push_back(masterList.GetStateAt(i));
    }

    for (WaveTrack* track : TrackList::Get(project).Any<WaveTrack>()) {
        auto& list = RealtimeEffectList::Get(*track);
        auto& stack = stacks[track->GetId()];
        stack.reserve(list.GetStatesCount());
        for (auto i = 0; i < static_cast<int>(list.GetStatesCount()); ++i) {
            stack.push_back(list.GetStateAt(i));
        }
    }

    return stacks;
}

/*!
 * \brief The AU3 `Track` objects have a `RealtimeEffectList` attached.
 * The `TrackListRestorer`, when adding a new history state, stores a copy of the track as well as all its attachments.
 * This way, when undoing/redoing, the RealtimeEffectService gets the list pointers of the previous/next state.
 * Here we do the same, but for the master track.
 */
class MasterEffectListRestorer final : public UndoStateExtension
{
public:
    MasterEffectListRestorer(au3::Au3Project& project)
        : m_targetState{getStacks(project)}, m_targetLists{effectLists(project)}
    {
    }

    void RestoreUndoRedoState(au3::Au3Project& project) override
    {
        auto& changed = RealtimeEffectStackChanged::Get(project).channel;
        auto& masterCurrentList = RealtimeEffectList::Get(project);
        auto& masterTargetList = *m_targetLists.at(IRealtimeEffectService::masterTrackId);
        if (masterTargetList != masterCurrentList) {
            masterTargetList.ShallowCopyTo(masterCurrentList);
            changed.send(IRealtimeEffectService::masterTrackId);
        }

        // TrackList& trackList = TrackList::Get(project);
        // for (WaveTrack* track : trackList.Any<WaveTrack>()) {
        //     auto& trackCurrentList = RealtimeEffectList::Get(*track);
        //     const auto it = m_targetLists.find(track->GetId());
        //     if (it == m_targetLists.end()) {
        //         // Not expected to happen.
        //         assert(false);
        //         changed.send(track->GetId());
        //     } else if (*it->second != trackCurrentList) {
        //         it->second->ShallowCopyTo(trackCurrentList);
        //         changed.send(track->GetId());
        //     }
        // }

        // const auto currentState = getStacks(project);
        // if (currentState == m_targetState) {
        //     return;
        // }

        // auto& changeChannel = RealtimeEffectStackChanged::Get(project).channel;

        // for (const auto& [trackId, targetStack] : m_targetState) {
        //     if (!currentState.count(trackId) || currentState.at(trackId) != targetStack) {
        //         changeChannel.send(trackId);
        //     }
        // }

        // for (const auto& [trackId, _] : currentState) {
        //     if (!m_targetState.count(trackId)) {
        //         changeChannel.send(trackId);
        //     }
        // }
    }

    muse::async::Channel<TrackId> realtimeEffectStackChanged;

private:
    const std::unordered_map<effects::TrackId, std::vector<RealtimeEffectStatePtr> > m_targetState;
    const std::unordered_map<effects::TrackId, std::unique_ptr<RealtimeEffectList> > m_targetLists;
};

static UndoRedoExtensionRegistry::Entry sEntry {
    [](au3::Au3Project& project) -> std::shared_ptr<UndoStateExtension>
    {
        return std::make_shared<MasterEffectListRestorer>(project);
    }
};
}

std::string RealtimeEffectService::getEffectName(const RealtimeEffectState& state) const
{
    return effectsProvider()->effectName(state.GetID().ToStdString());
}

void RealtimeEffectService::init()
{
    globalContext()->currentProjectChanged().onNotify(this, [this]
    {
        updateSubscriptions(globalContext()->currentProject());
    });

    updateSubscriptions(globalContext()->currentProject());
}

void RealtimeEffectService::updateSubscriptions(const au::project::IAudacityProjectPtr& project)
{
    m_rtEffectSubscriptions.clear();

    if (!project) {
        return;
    }

    auto au3Project = reinterpret_cast<au::au3::Au3Project*>(project->au3ProjectPtr());

    RealtimeEffectStackChanged::Get(*au3Project).channel = m_realtimeEffectStackChanged;

    auto& trackList = TrackList::Get(*au3Project);
    for (WaveTrack* track : trackList.Any<WaveTrack>()) {
        onWaveTrackAdded(*track);
    }

    m_tracklistSubscription = trackList.Subscribe([this, w = weak_from_this()](const TrackListEvent& e)
    {
        const auto lifeguard = w.lock();
        if (lifeguard) {
            onTrackListEvent(e);
        }
    });

    // Regular tracks get added to the project and we will get the notifications, but we have to do it proactively for the master track.
    m_rtEffectSubscriptions[masterTrackId] = subscribeToRealtimeEffectList(masterTrackId, RealtimeEffectList::Get(*au3Project));
    m_realtimeEffectStackChanged.send(masterTrackId);
}

void RealtimeEffectService::onTrackListEvent(const TrackListEvent& e)
{
    switch (e.mType) {
    case TrackListEvent::ADDITION: {
        if (const auto waveTrack = std::dynamic_pointer_cast<WaveTrack>(e.mpTrack.lock())) {
            onWaveTrackAdded(*waveTrack);
        }
    }
    break;
    case TrackListEvent::DELETION:
    {
        IF_ASSERT_FAILED(e.mId.has_value()) {
            return;
        }
        m_rtEffectSubscriptions.erase(*e.mId);
        m_realtimeEffectStackChanged.send(*e.mId);
    }
    break;
    }
}

void RealtimeEffectService::onWaveTrackAdded(WaveTrack& track)
{
    // Renew the subscription: `track` may have a registered ID, it may yet be a new object.
    auto& list = RealtimeEffectList::Get(track);
    m_rtEffectSubscriptions[track.GetId()] = subscribeToRealtimeEffectList(track.GetId(), list);
    m_realtimeEffectStackChanged.send(track.GetId());
}

Observer::Subscription RealtimeEffectService::subscribeToRealtimeEffectList(TrackId trackId, RealtimeEffectList& list)
{
    return list.Subscribe([this, trackId, weakThis = weak_from_this()](const RealtimeEffectListMessage& msg)
    {
        const auto lifeguard = weakThis.lock();
        if (!lifeguard) {
            return;
        }
        switch (msg.type) {
        case RealtimeEffectListMessage::Type::Insert:
        case RealtimeEffectListMessage::Type::Remove:
        case RealtimeEffectListMessage::Type::DidReplace:
        case RealtimeEffectListMessage::Type::Move:
            m_realtimeEffectStackChanged.send(trackId);
            break;
        }
    });
}

muse::async::Channel<TrackId> RealtimeEffectService::realtimeEffectStackChanged() const
{
    return m_realtimeEffectStackChanged;
}

std::optional<TrackId> RealtimeEffectService::trackId(const RealtimeEffectStatePtr& state) const
{
    const project::IAudacityProjectPtr project = globalContext()->currentProject();
    if (!project) {
        return {};
    }
    const auto au3Project = reinterpret_cast<au3::Au3Project*>(project->au3ProjectPtr());
    auto& trackList = TrackList::Get(*au3Project);
    for (WaveTrack* track : trackList.Any<WaveTrack>()) {
        if (RealtimeEffectList::Get(*track).FindState(state)) {
            return track->GetId();
        }
    }
    auto& masterList = RealtimeEffectList::Get(*au3Project);
    if (masterList.FindState(state)) {
        return masterTrackId;
    }
    return {};
}

std::optional<std::string> RealtimeEffectService::effectTrackName(const RealtimeEffectStatePtr& state) const
{
    const auto tId = trackId(state);
    if (!tId.has_value()) {
        return {};
    }
    return effectTrackName(*tId);
}

std::optional<EffectChainLinkIndex> RealtimeEffectService::effectIndex(const RealtimeEffectStatePtr& state) const
{
    const project::IAudacityProjectPtr project = globalContext()->currentProject();
    IF_ASSERT_FAILED(project) {
        return {};
    }
    const auto au3Project = reinterpret_cast<au3::Au3Project*>(project->au3ProjectPtr());
    auto& trackList = TrackList::Get(*au3Project);
    for (WaveTrack* track : trackList.Any<WaveTrack>()) {
        if (const std::optional<size_t> index = RealtimeEffectList::Get(*track).FindState(state)) {
            return static_cast<EffectChainLinkIndex>(*index);
        }
    }
    auto& masterList = RealtimeEffectList::Get(*au3Project);
    if (const std::optional<size_t> index = masterList.FindState(state)) {
        return static_cast<EffectChainLinkIndex>(*index);
    }
    return {};
}

std::optional<std::vector<RealtimeEffectStatePtr> > RealtimeEffectService::effectStack(TrackId trackId) const
{
    const auto data = utilData(trackId);
    if (!data) {
        return {};
    }
    std::vector<RealtimeEffectStatePtr> stack(data->effectList->GetStatesCount());
    for (auto i = 0; i < static_cast<int>(stack.size()); ++i) {
        stack[i] = data->effectList->GetStateAt(i);
    }
    return stack;
}

void RealtimeEffectService::reorderRealtimeEffect(const RealtimeEffectStatePtr& state, int newIndex)
{
    const auto tId = trackId(state);
    if (!tId.has_value()) {
        return;
    }
    const auto oldIndex = effectIndex(state);
    IF_ASSERT_FAILED(oldIndex.has_value()) {
        return;
    }
    RealtimeEffectList* const list = realtimeEffectList(*tId);
    list->MoveEffect(*oldIndex, newIndex);

    const auto effectName = getEffectName(*state);
    const auto trackName = effectTrackName(*tId);
    projectHistory()->pushHistoryState("Moved " + effectName
                                       + (oldIndex < newIndex ? " up " : " down ") + " in " + trackName.value_or(""),
                                       "Change effect order");
}

std::optional<std::string> RealtimeEffectService::effectTrackName(TrackId trackId) const
{
    if (trackId == masterTrackId) {
        return muse::TranslatableString("effects", "Master").translated().toStdString();
    }
    const auto data = utilData(trackId);
    if (data) {
        return data->trackeditProject->trackName(trackId);
    } else {
        return {};
    }
}

std::optional<RealtimeEffectService::UtilData> RealtimeEffectService::utilData(TrackId trackId) const
{
    const project::IAudacityProjectPtr project = globalContext()->currentProject();
    if (!project) {
        return {};
    }

    const auto au3Project = reinterpret_cast<au3::Au3Project*>(project->au3ProjectPtr());
    const auto isMasterTrack = trackId == masterTrackId;
    const auto au3Track
        = isMasterTrack ? nullptr : dynamic_cast<au3::Au3Track*>(au3::DomAccessor::findTrack(*au3Project,
                                                                                             au3::Au3TrackId(trackId)));
    const auto trackeditProject = globalContext()->currentTrackeditProject().get();
    const auto effectList = au3Track ? &RealtimeEffectList::Get(*au3Track) : &RealtimeEffectList::Get(*au3Project);

    if (!au3Project || !(isMasterTrack || au3Track) || !trackeditProject) {
        return {};
    }
    return UtilData{ au3Project, au3Track, trackeditProject, effectList };
}

RealtimeEffectStatePtr RealtimeEffectService::addRealtimeEffect(TrackId trackId, const muse::String& effectId)
{
    const auto data = utilData(trackId);
    IF_ASSERT_FAILED(data) {
        return nullptr;
    }

    if (const auto state = AudioIO::Get()->AddState(*data->au3Project, data->au3Track, effectId.toStdString())) {
        const auto effectName = getEffectName(*state);
        const auto trackName = effectTrackName(trackId);
        projectHistory()->pushHistoryState("Added " + effectName + " to " + trackName.value_or(""), "Add " + effectName);
        return state;
    }

    return nullptr;
}

namespace {
std::shared_ptr<RealtimeEffectState> findEffectState(RealtimeEffectList& effectList, RealtimeEffectStatePtr effectStateId)
{
    for (auto i = 0; i < effectList.GetStatesCount(); ++i) {
        const auto state = effectList.GetStateAt(i);
        if (state == effectStateId) {
            return state;
        }
    }
    return nullptr;
}
}

void RealtimeEffectService::removeRealtimeEffect(TrackId trackId, const RealtimeEffectStatePtr& effectStateId)
{
    const auto data = utilData(trackId);
    IF_ASSERT_FAILED(data) {
        return;
    }

    const auto state = findEffectState(*data->effectList, effectStateId);
    IF_ASSERT_FAILED(state) {
        return;
    }

    AudioIO::Get()->RemoveState(*data->au3Project, data->au3Track, state);
    const auto effectName = getEffectName(*state);
    const auto trackName = effectTrackName(trackId);
    projectHistory()->pushHistoryState("Removed " + effectName + " from " + trackName.value_or(""), "Remove " + effectName);
}

RealtimeEffectStatePtr RealtimeEffectService::replaceRealtimeEffect(TrackId trackId, int effectListIndex, const muse::String& newEffectId)
{
    const auto data = utilData(trackId);
    IF_ASSERT_FAILED(data) {
        return nullptr;
    }

    const auto oldState = data->effectList->GetStateAt(effectListIndex);
    if (const auto newState = AudioIO::Get()->ReplaceState(*data->au3Project, data->au3Track, effectListIndex, newEffectId.toStdString())) {
        const auto oldEffectName = getEffectName(*oldState);
        const auto newEffectName = getEffectName(*newState);
        projectHistory()->pushHistoryState("Replaced " + oldEffectName + " with " + newEffectName, "Replace " + oldEffectName);
        return newState;
    }

    return nullptr;
}

bool RealtimeEffectService::isActive(const RealtimeEffectStatePtr& stateId) const
{
    if (stateId == 0) {
        return false;
    }
    return stateId->GetSettings().extra.GetActive();
}

void RealtimeEffectService::setIsActive(const RealtimeEffectStatePtr& stateId, bool isActive)
{
    if (stateId->GetSettings().extra.GetActive() == isActive) {
        return;
    }
    stateId->SetActive(isActive);
    m_isActiveChanged.send(stateId);
}

muse::async::Channel<RealtimeEffectStatePtr> RealtimeEffectService::isActiveChanged() const
{
    return m_isActiveChanged;
}

bool RealtimeEffectService::trackEffectsActive(TrackId trackId) const
{
    const auto list = realtimeEffectList(trackId);
    return list ? list->IsActive() : false;
}

void RealtimeEffectService::setTrackEffectsActive(TrackId trackId, bool active)
{
    const auto list = realtimeEffectList(trackId);
    if (list) {
        list->SetActive(active);
    }
}

const RealtimeEffectList* RealtimeEffectService::realtimeEffectList(TrackId trackId) const
{
    const auto data = utilData(trackId);
    if (!data) {
        return nullptr;
    }
    return data->effectList;
}

RealtimeEffectList* RealtimeEffectService::realtimeEffectList(TrackId trackId)
{
    return const_cast<RealtimeEffectList*>(const_cast<const RealtimeEffectService*>(this)->realtimeEffectList(trackId));
}
}

// Inject a factory for realtime effects
static RealtimeEffectState::EffectFactory::Scope scope{ &EffectManager::GetInstanceFactory };
