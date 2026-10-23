#include "trackqueue.h"
#include "../api/jsonparser.h"

#include <QJsonObject>
#include <QJsonDocument>

TrackQueue::TrackQueue(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TrackQueue::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_tracks.size();
}

QVariant TrackQueue::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return QVariant();
    const ScTrack &t = m_tracks.at(index.row());
    switch (role) {
    case TitleRole: return t.title;
    case UsernameRole: return t.user.username;
    case ArtworkUrlRole: return t.artworkUrl;
    case DurationMsRole: return t.durationMs;
    case DurationTextRole: return t.durationText();
    case UrnRole: return t.urn;
    case PermalinkUrlRole: return t.permalinkUrl;
    case PlayableRole: return t.access != QLatin1String("blocked");
    case AccessRole: return t.access;
    case IsCurrentRole: return index.row() == m_current;
    default: return QVariant();
    }
}

QHash<int, QByteArray> TrackQueue::roleNames() const
{
    return {
        { TitleRole, "title" },
        { UsernameRole, "username" },
        { ArtworkUrlRole, "artworkUrl" },
        { DurationMsRole, "durationMs" },
        { DurationTextRole, "durationText" },
        { UrnRole, "urn" },
        { PermalinkUrlRole, "permalinkUrl" },
        { PlayableRole, "playable" },
        { AccessRole, "access" },
        { IsCurrentRole, "isCurrent" }
    };
}

void TrackQueue::setCurrentIndex(int idx)
{
    if (m_current == idx)
        return;
    const int old = m_current;
    m_current = idx;
    if (old >= 0 && old < m_tracks.size())
        emit dataChanged(index(old), index(old), { IsCurrentRole });
    if (m_current >= 0 && m_current < m_tracks.size())
        emit dataChanged(index(m_current), index(m_current), { IsCurrentRole });
    emit currentIndexChanged();
    emit currentTrackChanged();
}

QVariantMap TrackQueue::currentTrack() const
{
    if (m_current < 0 || m_current >= m_tracks.size())
        return QVariantMap();
    return m_tracks.at(m_current).toVariantMap();
}

static ScTrack trackFromMap(const QVariantMap &m)
{
    ScTrack t;
    t.id = m.value(QStringLiteral("id")).toLongLong();
    t.urn = m.value(QStringLiteral("urn")).toString();
    t.title = m.value(QStringLiteral("title")).toString();
    t.description = m.value(QStringLiteral("description")).toString();
    t.artworkUrl = m.value(QStringLiteral("artworkUrl")).toString();
    t.permalinkUrl = m.value(QStringLiteral("permalinkUrl")).toString();
    t.streamUrl = m.value(QStringLiteral("streamUrl")).toString();
    t.waveformUrl = m.value(QStringLiteral("waveformUrl")).toString();
    t.access = m.value(QStringLiteral("access")).toString();
    if (t.access.isEmpty())
        t.access = QStringLiteral("playable");
    t.durationMs = m.value(QStringLiteral("durationMs")).toLongLong();
    t.playbackCount = m.value(QStringLiteral("playbackCount")).toLongLong();
    t.likesCount = m.value(QStringLiteral("likesCount")).toLongLong();
    t.commentCount = m.value(QStringLiteral("commentCount")).toLongLong();
    t.userFavorite = m.value(QStringLiteral("userFavorite")).toBool();
    t.streamable = !m.contains(QStringLiteral("streamable"))
            || m.value(QStringLiteral("streamable")).toBool();
    const QVariantMap u = m.value(QStringLiteral("user")).toMap();
    t.user.id = u.value(QStringLiteral("id")).toLongLong();
    t.user.urn = u.value(QStringLiteral("urn")).toString();
    t.user.username = m.value(QStringLiteral("username")).toString();
    if (t.user.username.isEmpty())
        t.user.username = u.value(QStringLiteral("username")).toString();
    t.user.avatarUrl = u.value(QStringLiteral("avatarUrl")).toString();
    t.user.permalinkUrl = u.value(QStringLiteral("permalinkUrl")).toString();
    return t;
}

void TrackQueue::setFromList(const QVariantList &tracks, int startIndex)
{
    beginResetModel();
    m_tracks.clear();
    for (const QVariant &v : tracks)
        m_tracks.append(trackFromMap(v.toMap()));
    endResetModel();
    emit countChanged();
    setCurrentIndex(qBound(0, startIndex, m_tracks.isEmpty() ? -1 : m_tracks.size() - 1));
    if (m_tracks.isEmpty())
        setCurrentIndex(-1);
}

void TrackQueue::append(const QVariantMap &track)
{
    beginInsertRows(QModelIndex(), m_tracks.size(), m_tracks.size());
    m_tracks.append(trackFromMap(track));
    endInsertRows();
    emit countChanged();
}

void TrackQueue::insertNext(const QVariantMap &track)
{
    const int pos = (m_current < 0) ? 0 : m_current + 1;
    beginInsertRows(QModelIndex(), pos, pos);
    ScTrack t;
    t.id = track.value(QStringLiteral("id")).toLongLong();
    t.urn = track.value(QStringLiteral("urn")).toString();
    t.title = track.value(QStringLiteral("title")).toString();
    t.artworkUrl = track.value(QStringLiteral("artworkUrl")).toString();
    t.permalinkUrl = track.value(QStringLiteral("permalinkUrl")).toString();
    t.access = track.value(QStringLiteral("access")).toString();
    if (t.access.isEmpty())
        t.access = QStringLiteral("playable");
    t.durationMs = track.value(QStringLiteral("durationMs")).toLongLong();
    t.userFavorite = track.value(QStringLiteral("userFavorite")).toBool();
    t.user.username = track.value(QStringLiteral("username")).toString();
    m_tracks.insert(pos, t);
    endInsertRows();
    emit countChanged();
}

void TrackQueue::removeAt(int idx)
{
    if (idx < 0 || idx >= m_tracks.size())
        return;
    beginRemoveRows(QModelIndex(), idx, idx);
    m_tracks.removeAt(idx);
    endRemoveRows();
    emit countChanged();
    if (m_current >= m_tracks.size())
        setCurrentIndex(m_tracks.isEmpty() ? -1 : m_tracks.size() - 1);
}

void TrackQueue::move(int from, int to)
{
    if (from == to || from < 0 || from >= m_tracks.size()
            || to < 0 || to >= m_tracks.size())
        return;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    m_tracks.move(from, to);
    endMoveRows();
    // keep the current track identity stable
    if (m_current == from)
        m_current = to;
    else if (from < m_current && to >= m_current)
        --m_current;
    else if (from > m_current && to <= m_current)
        ++m_current;
    emit currentIndexChanged();
}

void TrackQueue::clear()
{
    beginResetModel();
    m_tracks.clear();
    endResetModel();
    emit countChanged();
    setCurrentIndex(-1);
}

QVariantMap TrackQueue::get(int idx) const
{
    if (idx < 0 || idx >= m_tracks.size())
        return QVariantMap();
    return m_tracks.at(idx).toVariantMap();
}

void TrackQueue::setFavorite(const QString &urn, bool favorite)
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].urn == urn) {
            m_tracks[i].userFavorite = favorite;
            const QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx);
            if (i == m_current)
                emit currentTrackChanged();
            break;
        }
    }
}

ScTrack TrackQueue::trackAt(int idx) const
{
    if (idx < 0 || idx >= m_tracks.size())
        return ScTrack();
    return m_tracks.at(idx);
}
