#include "au3trackeditproject.h"

#include "libraries/lib-wave-track/WaveTrack.h"
#include "libraries/lib-numeric-formats/ProjectTimeSignature.h"
#include "libraries/lib-project-history/ProjectHistory.h"
#include "libraries/lib-stretching-sequence/TempoChange.h"
#include "libraries/lib-realtime-effects/RealtimeEffectList.h"
#include "libraries/lib-realtime-effects/RealtimeEffectState.h"

#include "au3audio/iaudioengine.h"
#include "au3wrap/iau3project.h"
#include "au3wrap/internal/domconverter.h"
#include "au3wrap/internal/domaccessor.h"
#include "au3wrap/internal/wxtypes_convert.h"

#include "log.h"
#include "UndoManager.h"

#include <unordered_map>

using namespace muse;
using namespace au::trackedit;
using namespace au::au3;

class Au3TrackeditProject::ChannelGroupWrapper : public au::audio::IAudioSource
{
public:
    ChannelGroupWrapper(std::weak_ptr<ChannelGroup> group, TrackId trackId)
        : m_group{group}, m_numChannels{group.expired() ? 0 : static_cast<int>(group.lock()->NChannels())},
        m_trackId{trackId}
    {
    }

    int numChannels() const override
    {
        return m_numChannels;
    }

    TrackId trackId() const
    {
        return m_trackId;
    }

    bool expired() const
    {
        return m_group.expired();
    }

    const ChannelGroup* group() const
    {
        return m_group.lock().get();
    }

private:
    const std::weak_ptr<ChannelGroup> m_group;
    const int m_numChannels;
    const TrackId m_trackId;
};

struct Au3TrackeditProject::Au3Impl
{
    Au3Project* prj = nullptr;
    Au3TrackList* trackList = nullptr;

    // events
    Observer::Subscription tracksSubc;
    bool trackReplacing = false;
    Observer::Subscription projectTimeSignatureSubscription;
};

Au3TrackeditProject::Au3TrackeditProject(const std::shared_ptr<IAu3Project>& au3project)
    : m_au3project{au3project}
{
    m_impl = std::make_shared<Au3Impl>();
    m_impl->prj = reinterpret_cast<Au3Project*>(au3project->au3ProjectPtr());
    m_impl->trackList = &Au3TrackList::Get(*m_impl->prj);
    m_impl->tracksSubc = m_impl->trackList->Subscribe([this](const TrackListEvent& e) {
        onTrackListEvent(e);
    });
    m_impl->projectTimeSignatureSubscription = ProjectTimeSignature::Get(*m_impl->prj).Subscribe(
        [this](const TimeSignatureChangedMessage& event) {
        onProjectTempoChange(event.newTempo);
    });

    audioEngine()->realtimeEffectAppended().onReceive(this,
                                                      [this](const audio::IAudioSource* source, audio::EffectChainLinkIndex index,
                                                             audio::EffectChainLinkPtr item)
    {
        // find wrapper
        const auto it = std::find_if(m_wrappers.begin(), m_wrappers.end(), [source](const auto& wrapper) {
            return wrapper.get() == source;
        });
        if (it == m_wrappers.end()) {
            return;
        }
        m_realtimeEffectAdded.send((*it)->trackId(), index, std::move(item));
    });
}

Au3TrackeditProject::~Au3TrackeditProject()
{
    m_impl->tracksSubc.Reset();
}

std::vector<au::trackedit::TrackId> Au3TrackeditProject::trackIdList() const
{
    std::vector<au::trackedit::TrackId> au4trackIds;

    for (const Au3Track* t : *m_impl->trackList) {
        au4trackIds.push_back(t->GetId());
    }

    return au4trackIds;
}

std::vector<au::trackedit::Track> Au3TrackeditProject::trackList() const
{
    std::vector<au::trackedit::Track> au4tracks;

    for (const Au3Track* t : *m_impl->trackList) {
        Track au4t = DomConverter::track(t);
        au4tracks.push_back(std::move(au4t));
    }

    return au4tracks;
}

static std::string eventTypeToString(const TrackListEvent& e)
{
    switch (e.mType) {
    case TrackListEvent::SELECTION_CHANGE: return "SELECTION_CHANGE";
    case TrackListEvent::TRACK_DATA_CHANGE: return "TRACK_DATA_CHANGE";
    case TrackListEvent::PERMUTED: return "PERMUTED";
    case TrackListEvent::RESIZING: return "RESIZING";
    case TrackListEvent::ADDITION: return "ADDITION";
    case TrackListEvent::DELETION: return e.mExtra ? "REPLACING" : "DELETION";
    }
}

void Au3TrackeditProject::onTrackListEvent(const TrackListEvent& e)
{
    TrackId trackId = -1;
    auto track = e.mpTrack.lock();
    if (track) {
        trackId = track->GetId();
    }
    LOGD() << "trackId: " << trackId << ", type: " << eventTypeToString(e);

    eraseExpiredWrappers();

    switch (e.mType) {
    case TrackListEvent::DELETION: {
        if (e.mExtra == 1) {
            m_impl->trackReplacing = true;
        }
        eraseWrapper(trackId);
    } break;
    case TrackListEvent::ADDITION: {
        if (m_impl->trackReplacing) {
            onTrackDataChanged(trackId);
            m_impl->trackReplacing = false;
            eraseWrapper(trackId);
        }
        const auto group = std::dynamic_pointer_cast<ChannelGroup>(track);
        if (group) {
            m_wrappers.emplace_back(std::make_shared<ChannelGroupWrapper>(group, trackId));
            audioEngine()->registerAudioSource(*m_au3project, m_wrappers.back().get());
        }
    } break;
    default:
        break;
    }
}

void Au3TrackeditProject::eraseWrapper(const TrackId& trackId)
{
    const auto source = trackAsAudioSource(trackId);
    if (!source) {
        return;
    }
    audioEngine()->unregisterAudioSource(source.get());
    const auto it = std::find_if(m_wrappers.begin(), m_wrappers.end(), [&](const auto& wrapper) {
        return wrapper.get() == source.get();
    });
    if (it != m_wrappers.end()) {
        m_wrappers.erase(it);
    }
}

void Au3TrackeditProject::onTrackDataChanged(const TrackId& trackId)
{
    auto it = m_clipsChanged.find(trackId);
    if (it != m_clipsChanged.end()) {
        it->second.changed();
    }
}

void Au3TrackeditProject::onProjectTempoChange(double newTempo)
{
    Au3TrackList& tracks = Au3TrackList::Get(*m_impl->prj);
    for (auto track : tracks) {
        DoProjectTempoChange(*track, newTempo);
        TrackId trackId = track->GetId();
        onTrackDataChanged(trackId);
    }
}

void Au3TrackeditProject::eraseExpiredWrappers()
{
    m_wrappers.erase(std::remove_if(m_wrappers.begin(), m_wrappers.end(), [](const auto& wrapper) {
        return wrapper->expired();
    }), m_wrappers.end());
}

muse::async::NotifyList<au::trackedit::Clip> Au3TrackeditProject::clipList(const au::trackedit::TrackId& trackId) const
{
    const Au3WaveTrack* waveTrack = DomAccessor::findWaveTrack(*m_impl->prj, Au3TrackId(trackId));
    IF_ASSERT_FAILED(waveTrack) {
        return muse::async::NotifyList<au::trackedit::Clip>();
    }

    muse::async::NotifyList<au::trackedit::Clip> clips;
    for (const std::shared_ptr<const Au3WaveClip>& interval : waveTrack->Intervals()) {
        au::trackedit::Clip clip = DomConverter::clip(waveTrack, interval.get());
        clips.push_back(std::move(clip));
    }

    async::ChangedNotifier<Clip>& notifier = m_clipsChanged[trackId];
    clips.setNotify(notifier.notify());

    return clips;
}

au::audio::IAudioSourcePtr Au3TrackeditProject::trackAsAudioSource(const TrackId& trackId) const
{
    for (auto& wrapper : m_wrappers) {
        auto waveTrack = dynamic_cast<const Au3WaveTrack*>(wrapper->group());
        if (waveTrack && waveTrack->GetId() == trackId) {
            return wrapper;
        }
    }
    return nullptr;
}

std::string Au3TrackeditProject::trackName(const TrackId& trackId) const
{
    return au::au3::DomConverter::track(au::au3::DomAccessor::findTrack(*m_impl->prj, au::au3::Au3TrackId { trackId })).title.toStdString();
}

void Au3TrackeditProject::reload()
{
    m_tracksChanged.send(trackList());
}

void Au3TrackeditProject::notifyAboutTrackAdded(const Track& track)
{
    m_trackAdded.send(track);
}

void Au3TrackeditProject::notifyAboutTrackChanged(const Track& track)
{
    m_trackChanged.send(track);
}

void Au3TrackeditProject::notifyAboutTrackRemoved(const Track& track)
{
    m_trackRemoved.send(track);
}

void Au3TrackeditProject::notifyAboutTrackInserted(const Track& track, int pos)
{
    m_trackInserted.send(track, pos);
}

void Au3TrackeditProject::notifyAboutTrackMoved(const Track& track, int pos)
{
    return m_trackMoved.send(track, pos);
}

au::trackedit::Clip Au3TrackeditProject::clip(const ClipKey& key) const
{
    Au3WaveTrack* waveTrack = DomAccessor::findWaveTrack(*m_impl->prj, Au3TrackId(key.trackId));
    IF_ASSERT_FAILED(waveTrack) {
        return Clip();
    }

    std::shared_ptr<Au3WaveClip> au3Clip = DomAccessor::findWaveClip(waveTrack, key.clipId);
    IF_ASSERT_FAILED(au3Clip) {
        return Clip();
    }

    return DomConverter::clip(waveTrack, au3Clip.get());
}

void Au3TrackeditProject::notifyAboutClipChanged(const Clip& clip)
{
    async::ChangedNotifier<Clip>& notifier = m_clipsChanged[clip.key.trackId];
    notifier.itemChanged(clip);
}

void Au3TrackeditProject::notifyAboutClipRemoved(const Clip& clip)
{
    async::ChangedNotifier<Clip>& notifier = m_clipsChanged[clip.key.trackId];
    notifier.itemRemoved(clip);
}

void Au3TrackeditProject::notifyAboutClipAdded(const Clip& clip)
{
    async::ChangedNotifier<Clip>& notifer = m_clipsChanged[clip.key.trackId];
    notifer.itemAdded(clip);
}

au::trackedit::TimeSignature Au3TrackeditProject::timeSignature() const
{
    trackedit::TimeSignature result;

    ProjectTimeSignature& timeSig = ProjectTimeSignature::Get(*m_impl->prj);
    result.tempo = timeSig.GetTempo();
    result.lower = timeSig.GetLowerTimeSignature();
    result.upper = timeSig.GetUpperTimeSignature();

    return result;
}

void Au3TrackeditProject::setTimeSignature(const trackedit::TimeSignature& timeSignature)
{
    ProjectTimeSignature& timeSig = ProjectTimeSignature::Get(*m_impl->prj);
    timeSig.SetTempo(timeSignature.tempo);
    timeSig.SetUpperTimeSignature(timeSignature.upper);
    timeSig.SetLowerTimeSignature(timeSignature.lower);

    m_timeSignatureChanged.send(timeSignature);
}

muse::async::Channel<au::trackedit::TimeSignature> Au3TrackeditProject::timeSignatureChanged() const
{
    return m_timeSignatureChanged;
}

muse::async::Channel<std::vector<au::trackedit::Track> > Au3TrackeditProject::tracksChanged() const
{
    return m_tracksChanged;
}

muse::async::Channel<au::trackedit::Track> Au3TrackeditProject::trackAdded() const
{
    return m_trackAdded;
}

muse::async::Channel<au::trackedit::Track> Au3TrackeditProject::trackChanged() const
{
    return m_trackChanged;
}

muse::async::Channel<au::trackedit::Track> Au3TrackeditProject::trackRemoved() const
{
    return m_trackRemoved;
}

muse::async::Channel<au::trackedit::Track, int> Au3TrackeditProject::trackInserted() const
{
    return m_trackInserted;
}

muse::async::Channel<au::trackedit::Track, int> Au3TrackeditProject::trackMoved() const
{
    return m_trackMoved;
}

muse::async::Channel<au::trackedit::TrackId, au::audio::EffectChainLinkIndex,
                     au::audio::EffectChainLinkPtr> Au3TrackeditProject::realtimeEffectAdded() const
{
    return m_realtimeEffectAdded;
}

secs_t Au3TrackeditProject::totalTime() const
{
    return m_impl->trackList->GetEndTime();
}

ITrackeditProjectPtr Au3TrackeditProjectCreator::create(const std::shared_ptr<IAu3Project>& au3project) const
{
    return std::make_shared<Au3TrackeditProject>(au3project);
}
