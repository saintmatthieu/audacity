/*
* Audacity: A Digital Audio Editor
*/
#include "trackslistclipsmodel.h"

#include "global/containers.h"

using namespace au::projectscene;

TracksListClipsModel::TracksListClipsModel(QObject* parent)
    : QAbstractListModel(parent)
{
    configuration()->isVerticalRulersVisibleChanged().onReceive(this, [this](bool isVerticalRulersVisible){
        setIsVerticalRulersVisible(isVerticalRulersVisible);
    });
}

void TracksListClipsModel::load()
{
    au::trackedit::ITrackeditProjectPtr prj = globalContext()->currentTrackeditProject();
    if (!prj) {
        return;
    }

    setIsVerticalRulersVisible(configuration()->isVerticalRulersVisible());

    beginResetModel();

    m_trackList = prj->trackList();

    m_dataSelectedTracks = selectionController()->dataSelectedOnTracks();
    selectionController()->dataSelectedOnTracksChanged().onReceive(this, [this](const std::vector<trackedit::TrackId>& tracks) {
        setDataSelectedTracks(tracks);
    });

    m_trackList.onItemAdded(this, [this](const trackedit::Track& track) {
        beginInsertRows(QModelIndex(), m_trackList.size(), m_trackList.size());
        m_trackList.push_back(track);
        endInsertRows();
    });

    m_trackList.onItemRemoved(this, [](const trackedit::Track& track) {
        Q_UNUSED(track);
        NOT_IMPLEMENTED;
    });

    m_trackList.onItemChanged(this, [](const trackedit::Track& track) {
        Q_UNUSED(track);
        NOT_IMPLEMENTED;
    });

    endResetModel();
}

int TracksListClipsModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(m_trackList.size());
}

QVariant TracksListClipsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const au::trackedit::Track& track = m_trackList.at(index.row());
    switch (role) {
    case TrackIdRole:
        return QVariant::fromValue(track.id);
    case IsDataSelectedRole: {
        return muse::contains(m_dataSelectedTracks, track.id);
    }
    default:
        break;
    }

    return QVariant();
}

QHash<int, QByteArray> TracksListClipsModel::roleNames() const
{
    static QHash<int, QByteArray> roles
    {
        //{ TypeRole, "trackType" },
        { TrackIdRole, "trackId" },
        { IsDataSelectedRole, "isDataSelected" }
    };
    return roles;
}

bool TracksListClipsModel::isVerticalRulersVisible() const
{
    return m_isVerticalRulersVisible;
}

void TracksListClipsModel::setIsVerticalRulersVisible(bool isVerticalRulersVisible)
{
    if (m_isVerticalRulersVisible == isVerticalRulersVisible) {
        return;
    }

    m_isVerticalRulersVisible = isVerticalRulersVisible;
    emit isVerticalRulersVisibleChanged(m_isVerticalRulersVisible);
}

void TracksListClipsModel::setDataSelectedTracks(const std::vector<trackedit::TrackId>& tracks)
{
    if (m_dataSelectedTracks == tracks) {
        return;
    }
    m_dataSelectedTracks = tracks;
    emit dataSelectedTracksChanged();
    emit dataChanged(index(0), index(m_trackList.size() - 1), { IsDataSelectedRole });
}
