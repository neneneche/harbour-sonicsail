#include "trackmodel.h"
#include "../api/apiclient.h"
#include "../api/jsonparser.h"

#include <QJsonObject>
#include <QDebug>

TrackModel::TrackModel(ApiClient *api, QObject *parent)
    : QAbstractListModel(parent)
    , m_api(api)
    , m_tagPrefix(QStringLiteral("tm%1-").arg(quintptr(this), 0, 16))
{
    connect(m_api, &ApiClient::listReceived, this, &TrackModel::onList);
    connect(m_api, &ApiClient::success, this, &TrackModel::onSuccess);
    connect(m_api, &ApiClient::failed, this, &TrackModel::onFailed);
}

bool TrackModel::ownsTag(const QString &tag) const
{
    return tag.startsWith(m_tagPrefix);
}

QString TrackModel::tagFor(const QString &kind) const
{
    return m_tagPrefix + kind;
}

int TrackModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_items.size();
}

QVariant TrackModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return QVariant();
    const QVariantMap m = m_items.at(index.row()).toMap();
    switch (role) {
    case TitleRole: return m.value(QStringLiteral("title"));
    case UsernameRole: return m.value(QStringLiteral("username"));
    case ArtworkUrlRole: return m.value(QStringLiteral("artworkUrl"));
    case DurationMsRole: return m.value(QStringLiteral("durationMs"));
    case DurationTextRole: return m.value(QStringLiteral("durationText"));
    case UrnRole: return m.value(QStringLiteral("urn"));
    case PermalinkUrlRole: return m.value(QStringLiteral("permalinkUrl"));
    case PlayableRole: return m.value(QStringLiteral("playable"));
    case AccessRole: return m.value(QStringLiteral("access"));
    case KindRole: return m.contains(QStringLiteral("trackCount")) ? QStringLiteral("playlist")
                                                                   : QStringLiteral("track");
    case PlaybackCountRole: return m.value(QStringLiteral("playbackCount"));
    case LikesCountRole: return m.value(QStringLiteral("likesCount"));
    case UserFavoriteRole: return m.value(QStringLiteral("userFavorite"));
    default: return QVariant();
    }
}

QHash<int, QByteArray> TrackModel::roleNames() const
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
        { KindRole, "kind" },
        { PlaybackCountRole, "playbackCount" },
        { LikesCountRole, "likesCount" },
        { UserFavoriteRole, "userFavorite" }
    };
}

void TrackModel::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void TrackModel::setError(const QString &e)
{
    if (m_error == e)
        return;
    m_error = e;
    emit errorStringChanged();
}

void TrackModel::resetTo(const QVariantList &items, const QString &nextHref)
{
    beginResetModel();
    m_items = items;
    endResetModel();
    m_nextHref = nextHref;
    emit countChanged();
    emit hasMoreChanged();
}

void TrackModel::appendItems(const QVariantList &items, const QString &nextHref)
{
    if (items.isEmpty()) {
        m_nextHref = nextHref;
        emit hasMoreChanged();
        return;
    }
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size() + items.size() - 1);
    for (const QVariant &v : items)
        m_items.append(v);
    endInsertRows();
    m_nextHref = nextHref;
    emit countChanged();
    emit hasMoreChanged();
}

void TrackModel::requestNextHydrateBatch()
{
    if (m_plHydrateQueue.isEmpty()) {
        // all batches done — assemble the ordered track list
        QVariantList out;
        for (const QVariant &v : m_plOrder) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("placeholder")).toBool()) {
                const QString id = QString::number(qint64(m.value(QStringLiteral("id")).toDouble()));
                const QVariantMap t = m_plHydrated.value(id);
                if (!t.isEmpty())
                    out.append(t);
            } else {
                out.append(m);
            }
        }
        m_plOrder.clear();
        m_plHydrated.clear();
        m_plHydrateQueue.clear();
        setBusy(false);
        setError(QString());
        resetTo(out, QString());
        return;
    }
    const QStringList ids = m_plHydrateQueue.takeFirst();
    QVariantMap q;
    q.insert(QStringLiteral("ids"), ids.join(QLatin1Char(',')));
    setBusy(true);
    m_api->get(QStringLiteral("/tracks"), q, tagFor(QStringLiteral("pl-hydrate")));
}

void TrackModel::requestHomeFeed()
{
    // SoundCloud home feed: new tracks and reposts from artists you follow
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 50);
    q.insert(QStringLiteral("linked_partitioning"), 1);
    m_api->get(QStringLiteral("/stream"), q, tagFor(QStringLiteral("list")));
}

// Queue cover fetches for artwork-less playlist tiles (mixes, stations,
// genre mixes — shelf AND feed items alike). Results are applied to the
// visible rows once fetched (2x2 mosaic from the first tracks' covers).
void TrackModel::queueCoverFetches(const QVariantList &items)
{
    m_mixArtRows.clear();
    m_mixArtPaths.clear();
    m_mixArtIds.clear();
    for (int i = 0; i < items.size(); ++i) {
        const QVariantMap m = items.at(i).toMap();
        if (!m.value(QStringLiteral("artworkUrl")).toString().isEmpty())
            continue;
        if (m_mixArtRows.size() >= 8)
            break;
        const QString kind = m.value(QStringLiteral("kind")).toString();
        QString urn = m.value(QStringLiteral("urn")).toString();
        const QString id = QString::number(qint64(m.value(QStringLiteral("id")).toDouble()));
        if (id.isEmpty() || id == QLatin1String("0"))
            continue;

        QString path;
        if (kind == QLatin1String("system-playlist")
                || urn.contains(QLatin1String("system-playlists"))) {
            // full URN form only (short form → 400)
            if (!urn.contains(QLatin1String("system-playlists")))
                urn = QStringLiteral("soundcloud:system-playlists:") + id;
            else if (!urn.startsWith(QLatin1String("soundcloud:")))
                urn.prepend(QStringLiteral("soundcloud:"));
            path = QStringLiteral("/system-playlists/") + urn;
        } else {
            // plain playlist: object endpoint embeds the first full tracks
            path = QStringLiteral("/playlists/") + id;
        }
        m_mixArtRows.append(i);
        m_mixArtPaths.append(path);
        m_mixArtIds.append(id);
    }
    processNextMixArt();
}

void TrackModel::processNextMixArt()
{
    if (m_mixArtPaths.isEmpty()) {
        m_mixArtRow = -1;
        return;
    }
    m_mixArtRow = m_mixArtRows.takeFirst();
    m_mixArtPath = m_mixArtPaths.takeFirst();
    m_mixArtItemId = m_mixArtIds.takeFirst();
    m_mixArtArts.clear();
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 5);
    q.insert(QStringLiteral("representation"), QStringLiteral("full"));
    m_api->get(m_mixArtPath, q, tagFor(QStringLiteral("home-mix-art")));
}

void TrackModel::setMixArtwork(int row, const QString &itemId, const QVariantList &arts)
{
    if (arts.isEmpty() || row < 0 || row >= m_items.size())
        return;
    QVariantMap m = m_items.at(row).toMap();
    // stale response (list was reloaded) → ignore
    const QString id = QString::number(qint64(m.value(QStringLiteral("id")).toDouble()));
    if (!itemId.isEmpty() && id != itemId)
        return;
    m.insert(QStringLiteral("artworkCandidates"), arts);
    m_items[row] = m;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
}

void TrackModel::beginLoadAll()
{
    m_loadAll = true;
    m_autoPages = 0;
    m_lastNextHref.clear();
}

void TrackModel::continuePagination(const QString &tag, const QString &nextHref)
{
    // Follow linked_partitioning next_href until the source is exhausted.
    // limit=200 is only the HTTP page size — not a total cap.
    if (!m_loadAll || nextHref.isEmpty()) {
        setBusy(false);
        return;
    }
    // loop guard only: stop if the API repeats the same next_href
    if (nextHref == m_lastNextHref) {
        qWarning() << "TrackModel: repeated next_href, stopping" << nextHref;
        setBusy(false);
        return;
    }
    m_lastNextHref = nextHref;
    ++m_autoPages;
    m_pageTag = tag;
    m_appendMode = true;
    setBusy(true);
    m_api->getMore(nextHref, tag);
}

void TrackModel::onList(const QString &tag, const QVariantList &items, const QString &nextHref)
{
    if (!ownsTag(tag))
        return;
    const QString kind = tag.mid(m_tagPrefix.size());

    // /tracks?ids= batch hydrating mosaic covers (JSON array → onList)
    if (kind == QLatin1String("home-mix-art-hydrate")) {
        for (const QVariant &v : items) {
            if (m_mixArtArts.size() >= 4)
                break;
            QVariantMap raw = v.toMap();
            if (raw.contains(QStringLiteral("track")))
                raw = raw.value(QStringLiteral("track")).toMap();
            QString art = raw.value(QStringLiteral("artwork_url")).toString();
            if (art.isEmpty())
                art = raw.value(QStringLiteral("artworkUrl")).toString();
            if (art.isEmpty())
                art = raw.value(QStringLiteral("user")).toMap()
                        .value(QStringLiteral("avatar_url")).toString();
            if (!art.isEmpty())
                m_mixArtArts.append(art);
        }
        setMixArtwork(m_mixArtRow, m_mixArtItemId, m_mixArtArts);
        processNextMixArt();
        return;
    }

    // /tracks?ids= batch → raw track objects hydrating playlist stubs
    if (kind == QLatin1String("pl-hydrate")) {
        for (const QVariant &v : items) {
            const QVariantMap raw = v.toMap();
            if (raw.isEmpty())
                continue;
            const QVariantMap norm = JsonParser::trackMap(QJsonObject::fromVariantMap(raw));
            const QString id = QString::number(qint64(norm.value(QStringLiteral("id")).toDouble()));
            if (id != QLatin1String("0"))
                m_plHydrated.insert(id, norm);
        }
        requestNextHydrateBatch();
        return;
    }

    setError(QString());
    QVariantList use = items;
    // first Home page: shelf of mixes above the feed
    if (m_lastKind == QLatin1String("home") && !m_appendMode) {
        if (!m_homeMixes.isEmpty()) {
            use = m_homeMixes + use;
            m_homeMixes.clear();
        }
        // artwork-less mix tiles (shelf AND feed) get 2x2 cover mosaics
        queueCoverFetches(use);
    }
    if (m_lastKind == QLatin1String("likes")) {
        use.clear();
        for (const QVariant &v : items) {
            QVariantMap m = v.toMap();
            const QString k = m.value(QStringLiteral("kind")).toString();
            if (k == QLatin1String("playlist") || k == QLatin1String("system-playlist")
                    || m.contains(QStringLiteral("trackCount")))
                continue;
            // everything in the likes list is liked
            m.insert(QStringLiteral("userFavorite"), true);
            use.append(m);
        }
    }
    // playlist tab sources (library / created / saved) — keep paging each chain
    if (m_lastKind == QLatin1String("user-playlists")
            && (kind == QLatin1String("me-pl-lib")
                || kind == QLatin1String("me-pl-created")
                || kind == QLatin1String("me-pl-likes"))) {
        collectPlaylists(items);
        if (!nextHref.isEmpty()) {
            // this source has more pages — fetch them before counting it done
            m_api->getMore(nextHref, tag);
            return;
        }
        if (!m_plPageBusy.contains(kind)) {
            m_plPageBusy.insert(kind); // mark this source complete
            if (m_plPending > 0)
                --m_plPending;
        }
        finishPlaylists();
        return;
    }
    if (m_appendMode)
        appendItems(use, nextHref);
    else
        resetTo(use, nextHref);
    m_appendMode = false;
    if (m_loadAll && !nextHref.isEmpty()) {
        continuePagination(tag, nextHref);
        return;
    }
    setBusy(false);
}

void TrackModel::onSuccess(const QString &tag, const QVariantMap &data)
{
    if (!ownsTag(tag))
        return;
    const QString kind = tag.mid(m_tagPrefix.size());

    // GET /playlists/{id}?representation=full → object with tracks[].
    // Only ~5 entries are full objects; the rest are id-only stubs that must
    // be batch-hydrated via /tracks?ids= (the /playlists/{id}/tracks
    // subresource no longer exists).
    if (kind == QLatin1String("pl-obj") || kind == QLatin1String("pl")) {
        m_plOrder.clear();
        m_plHydrateQueue.clear();
        m_plHydrated.clear();
        QStringList stubIds;
        const QVariantList tracks = data.value(QStringLiteral("tracks")).toList();
        for (const QVariant &tv : tracks) {
            QVariantMap raw = tv.toMap();
            if (raw.contains(QStringLiteral("track")))
                raw = raw.value(QStringLiteral("track")).toMap();
            if (raw.isEmpty())
                continue;
            const bool isStub = raw.value(QStringLiteral("title")).toString().isEmpty()
                    && raw.value(QStringLiteral("permalink_url")).toString().isEmpty();
            if (isStub) {
                const qint64 id = qint64(raw.value(QStringLiteral("id")).toDouble());
                if (id <= 0)
                    continue;
                QVariantMap placeholder;
                placeholder.insert(QStringLiteral("id"), double(id));
                placeholder.insert(QStringLiteral("placeholder"), true);
                m_plOrder.append(placeholder);
                const QString sid = QString::number(id);
                if (!stubIds.contains(sid))
                    stubIds.append(sid);
            } else {
                m_plOrder.append(JsonParser::trackMap(QJsonObject::fromVariantMap(raw)));
            }
        }

        // legacy shapes without tracks[] — plain collection parse
        if (m_plOrder.isEmpty() && stubIds.isEmpty()) {
            QVariantList items;
            const QJsonObject root = QJsonObject::fromVariantMap(data);
            items = JsonParser::parseCollection(root);
            if (items.isEmpty()) {
                const QVariantList col = data.value(QStringLiteral("collection")).toList();
                for (const QVariant &tv : col) {
                    QVariantMap raw = tv.toMap();
                    if (raw.contains(QStringLiteral("track")))
                        raw = raw.value(QStringLiteral("track")).toMap();
                    if (!raw.isEmpty() && !raw.value(QStringLiteral("title")).toString().isEmpty())
                        items.append(JsonParser::trackMap(QJsonObject::fromVariantMap(raw)));
                }
            }
            setBusy(false);
            setError(QString());
            resetTo(items, QString());
            return;
        }

        if (!stubIds.isEmpty()) {
            for (int i = 0; i < stubIds.size(); i += 50)
                m_plHydrateQueue.append(stubIds.mid(i, 50));
            requestNextHydrateBatch();
            return;
        }

        setBusy(false);
        setError(QString());
        resetTo(m_plOrder, QString());
        m_plOrder.clear();
        return;
    }

    // /mixed-selections → "Made for you" shelf (system playlists / mixes)
    if (kind == QLatin1String("home-mixes")) {
        const QJsonObject root = QJsonObject::fromVariantMap(data);
        m_homeMixes = JsonParser::parseCollection(root);
        requestHomeFeed();
        return;
    }

    // artwork fetch for a mix without artwork → 2x2 mosaic candidates
    if (kind == QLatin1String("home-mix-art")) {
        m_mixArtArts.clear();
        QStringList stubIds;
        const QVariantList tracks = data.value(QStringLiteral("tracks")).toList();
        for (const QVariant &tv : tracks) {
            QVariantMap raw = tv.toMap();
            if (raw.contains(QStringLiteral("track")))
                raw = raw.value(QStringLiteral("track")).toMap();
            QString art = raw.value(QStringLiteral("artwork_url")).toString();
            if (art.isEmpty())
                art = raw.value(QStringLiteral("artworkUrl")).toString();
            if (art.isEmpty())
                art = raw.value(QStringLiteral("user")).toMap()
                        .value(QStringLiteral("avatar_url")).toString();
            if (!art.isEmpty() && m_mixArtArts.size() < 4)
                m_mixArtArts.append(art);
            else if (art.isEmpty() && stubIds.size() < 4) {
                const qint64 id = qint64(raw.value(QStringLiteral("id")).toDouble());
                if (id > 0)
                    stubIds.append(QString::number(id));
            }
        }
        // system-playlist track lists are id-only stubs — hydrate covers
        if (m_mixArtArts.size() < 4 && !stubIds.isEmpty()) {
            QVariantMap q;
            q.insert(QStringLiteral("ids"), stubIds.join(QLatin1Char(',')));
            m_api->get(QStringLiteral("/tracks"), q, tagFor(QStringLiteral("home-mix-art-hydrate")));
            return;
        }
        setMixArtwork(m_mixArtRow, m_mixArtItemId, m_mixArtArts);
        processNextMixArt();
        return;
    }

    // /me/library/playlists, /users/{id}/playlists, /users/{id}/playlist_likes → merge
    if (kind == QLatin1String("me-pl-lib") || kind == QLatin1String("me-pl-created")
            || kind == QLatin1String("me-pl-likes")) {
        const QJsonObject root = QJsonObject::fromVariantMap(data);
        QVariantList items = JsonParser::parseCollection(root);
        if (items.isEmpty()) {
            const QVariantList col = data.value(QStringLiteral("collection")).toList();
            for (const QVariant &tv : col) {
                QVariantMap raw = tv.toMap();
                if (raw.contains(QStringLiteral("playlist")) && !raw.contains(QStringLiteral("title")))
                    raw = raw.value(QStringLiteral("playlist")).toMap();
                if (!raw.isEmpty())
                    items.append(JsonParser::playlistMap(QJsonObject::fromVariantMap(raw)));
            }
        }
        if (m_lastKind == QLatin1String("user-playlists")) {
            collectPlaylists(items);
            const QString next = data.value(QStringLiteral("next_href")).toString();
            if (!next.isEmpty()) {
                m_api->getMore(next, tag);
                return;
            }
            if (!m_plPageBusy.contains(kind)) {
                m_plPageBusy.insert(kind);
                if (m_plPending > 0)
                    --m_plPending;
            }
            finishPlaylists();
            return;
        }
        setBusy(false);
        setError(QString());
        resetTo(items, data.value(QStringLiteral("next_href")).toString());
        return;
    }

    // /me → then library + created + playlist_likes
    if (kind == QLatin1String("me-pl-owner")) {
        const QString id = QString::number(qint64(data.value(QStringLiteral("id")).toDouble()));
        if (id.isEmpty() || id == QLatin1String("0")) {
            setBusy(false);
            resetTo(QVariantList(), QString());
            return;
        }
        QVariantMap q;
        q.insert(QStringLiteral("limit"), 200);
        q.insert(QStringLiteral("linked_partitioning"), 1);
        m_plPending = 2;
        // created playlists (own uploads' playlists)
        m_api->get(QStringLiteral("/users/%1/playlists").arg(id), q,
                   tagFor(QStringLiteral("me-pl-created")));
        // saved / liked playlists from other users
        // (/me/library/* and /playlists/{id}/tracks were removed server-side;
        // playlist_likes still returns full playlist objects)
        m_api->get(QStringLiteral("/users/%1/playlist_likes").arg(id), q,
                   tagFor(QStringLiteral("me-pl-likes")));
        return;
    }

    // /me → then users/{id}/likes
    if (kind == QLatin1String("me")) {
        const QString id = QString::number(qint64(data.value(QStringLiteral("id")).toDouble()));
        if (id.isEmpty() || id == QLatin1String("0")) {
            setBusy(false);
            setError(tr("Could not load profile"));
            return;
        }
        QVariantMap q;
        q.insert(QStringLiteral("limit"), 200);
        q.insert(QStringLiteral("linked_partitioning"), 1);
        m_api->get(QStringLiteral("/users/%1/likes").arg(id), q, tagFor(QStringLiteral("list")));
        return;
    }

    setBusy(false);
    setError(QString());
    QVariantList items;
    const QVariantList tracks = data.value(QStringLiteral("tracks")).toList();
    for (const QVariant &tv : tracks) {
        QVariantMap raw = tv.toMap();
        if (raw.contains(QStringLiteral("track")))
            raw = raw.value(QStringLiteral("track")).toMap();
        if (raw.isEmpty())
            continue;
        items.append(JsonParser::trackMap(QJsonObject::fromVariantMap(raw)));
    }
    if (items.isEmpty()) {
        const QVariantList col = data.value(QStringLiteral("collection")).toList();
        for (const QVariant &tv : col) {
            QVariantMap raw = tv.toMap();
            if (raw.contains(QStringLiteral("track")))
                raw = raw.value(QStringLiteral("track")).toMap();
            if (!raw.isEmpty())
                items.append(JsonParser::trackMap(QJsonObject::fromVariantMap(raw)));
        }
    }
    resetTo(items, QString());
}

void TrackModel::onFailed(const QString &tag, int status, const QString &message)
{
    if (!ownsTag(tag))
        return;
    const QString kind = tag.mid(m_tagPrefix.size());
    setBusy(false);
    m_appendMode = false;

    // a hydration batch failed → skip it, continue with the rest
    if (kind == QLatin1String("pl-hydrate")) {
        requestNextHydrateBatch();
        return;
    }

    // mixes shelf failed (e.g. signed out) → continue with the feed anyway
    if (kind == QLatin1String("home-mixes")) {
        requestHomeFeed();
        return;
    }

    // one mix-artwork fetch failed → continue
    if (kind == QLatin1String("home-mix-art") || kind == QLatin1String("home-mix-art-hydrate")) {
        processNextMixArt();
        return;
    }

    // mixes failed → charts resolve → generic search
    if (m_lastKind == QLatin1String("mixes") && (status == 404 || status == 400 || status == -1 || status == 500)) {
        QVariantMap q;
        q.insert(QStringLiteral("url"),
                 QStringLiteral("https://soundcloud.com/discover/sets/charts-top:all-music:de"));
        m_lastKind = QStringLiteral("charts");
        m_api->get(QStringLiteral("/resolve"), q, tagFor(QStringLiteral("list")));
        return;
    }
    if (m_lastKind == QLatin1String("charts") && (status == 404 || status == 400 || status == -1)) {
        m_lastKind = QStringLiteral("search");
        QVariantMap q;
        q.insert(QStringLiteral("q"), QStringLiteral("music"));
        q.insert(QStringLiteral("limit"), 50);
        q.insert(QStringLiteral("linked_partitioning"), 1);
        m_api->get(QStringLiteral("/search/tracks"), q, tagFor(QStringLiteral("list")));
        return;
    }

    // history 404/500 → empty, no scary error
    if (m_lastKind == QLatin1String("history") && (status == 404 || status == 500)) {
        resetTo(QVariantList(), QString());
        return;
    }

    // home feed (401 when signed out, or empty feed) → shelf of mixes if any,
    // otherwise empty with the placeholder in UI
    if (m_lastKind == QLatin1String("home")) {
        const QVariantList mixes = m_homeMixes;
        m_homeMixes.clear();
        resetTo(mixes, QString());
        return;
    }

    if (kind == QLatin1String("me")) {
        setError(tr("Sign in to see likes"));
        return;
    }

    // one of the playlist sources failed → show whatever we got
    if (kind == QLatin1String("me-pl") || kind == QLatin1String("me-pl-lib")
            || kind == QLatin1String("me-pl-created")
            || kind == QLatin1String("me-pl-likes")) {
        if (m_lastKind == QLatin1String("user-playlists")) {
            if (!m_plPageBusy.contains(kind)) {
                m_plPageBusy.insert(kind);
                if (m_plPending > 0)
                    --m_plPending;
            }
            finishPlaylists();
            return;
        }
        setError(message.isEmpty() ? tr("Not found") : message);
        return;
    }

    // owner resolve failed → empty playlists (MainPage shows sign-in placeholder)
    if (kind == QLatin1String("me-pl-owner")) {
        resetTo(QVariantList(), QString());
        return;
    }

    setError(message.isEmpty() ? tr("Not found") : message);
}

void TrackModel::search(const QString &query)
{
    m_lastKind = QStringLiteral("search");
    m_lastQuery = query;
    m_lastArg.clear();
    m_appendMode = false;
    beginLoadAll();
    setBusy(true);
    setError(QString());
    QVariantMap q;
    q.insert(QStringLiteral("q"), query);
    q.insert(QStringLiteral("limit"), 200);
    q.insert(QStringLiteral("linked_partitioning"), 1);
    q.insert(QStringLiteral("offset"), 0);
    m_api->get(QStringLiteral("/search/tracks"), q, tagFor(QStringLiteral("list")));
}

void TrackModel::clearItems()
{
    setBusy(false);
    setError(QString());
    m_loadAll = false;
    m_autoPages = 0;
    m_plOrder.clear();
    m_plHydrateQueue.clear();
    m_plHydrated.clear();
    resetTo(QVariantList(), QString());
}

void TrackModel::loadUserPlaylists()
{
    m_lastKind = QStringLiteral("user-playlists");
    m_lastQuery.clear();
    m_lastArg.clear();
    m_appendMode = false;
    m_loadAll = false;
    m_plCollected.clear();
    m_plSeen.clear();
    m_plPageBusy.clear();
    m_plPending = 0;
    setBusy(true);
    setError(QString());
    // Resolve owner first so we can merge library + created + saved playlists
    m_api->get(QStringLiteral("/me"), QVariantMap(),
               tagFor(QStringLiteral("me-pl-owner")));
}

void TrackModel::collectPlaylists(const QVariantList &items)
{
    for (const QVariant &v : items) {
        QVariantMap m = v.toMap();
        // unwrap like / library wrappers
        if (m.contains(QStringLiteral("playlist")) && !m.contains(QStringLiteral("title")))
            m = m.value(QStringLiteral("playlist")).toMap();
        const QString kind = m.value(QStringLiteral("kind")).toString();
        // only real playlists — never system-playlist / tracks
        if (kind == QLatin1String("system-playlist") || kind == QLatin1String("track"))
            continue;
        const bool looksPlaylist = kind == QLatin1String("playlist")
                || m.contains(QStringLiteral("trackCount"))
                || m.contains(QStringLiteral("track_count"))
                || (!m.value(QStringLiteral("title")).toString().isEmpty()
                    && m.contains(QStringLiteral("permalinkUrl")));
        if (!looksPlaylist)
            continue;
        const QString key = m.value(QStringLiteral("urn")).toString();
        QString id = key;
        if (id.isEmpty())
            id = QString::number(qint64(m.value(QStringLiteral("id")).toDouble()));
        if (id.isEmpty() || id == QLatin1String("0") || m_plSeen.contains(id))
            continue;
        m_plSeen.insert(id);
        m_plCollected.append(m);
    }
}

void TrackModel::finishPlaylists()
{
    if (m_plPending > 0)
        return;
    setBusy(false);
    setError(QString());
    resetTo(m_plCollected, QString());
    m_plCollected.clear();
    m_plSeen.clear();
    m_plPageBusy.clear();
}

void TrackModel::loadHome()
{
    m_lastKind = QStringLiteral("home");
    m_lastQuery.clear();
    m_lastArg.clear();
    m_appendMode = false;
    m_homeMixes.clear();
    beginLoadAll();
    setBusy(true);
    setError(QString());
    // 1) "Made for you" shelf (Daily Drops, Weekly Wave, … system playlists)
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 20);
    m_api->get(QStringLiteral("/mixed-selections"), q, tagFor(QStringLiteral("home-mixes")));
    // /stream is requested once the shelf response is known (success or failure)
}

void TrackModel::loadMixes()
{
    m_lastKind = QStringLiteral("mixes");
    m_lastQuery.clear();
    m_lastArg.clear();
    m_appendMode = false;
    setBusy(true);
    setError(QString());
    // Prefer the signed-in user's own playlists (their SoundCloud library).
    // mixed-selections is curated discover content, not personal mixes.
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 50);
    q.insert(QStringLiteral("linked_partitioning"), 1);
    m_api->get(QStringLiteral("/me/playlists"), q, tagFor(QStringLiteral("me-pl-lib")));
}

void TrackModel::loadCharts()
{
    // alias: home is mixes now
    loadMixes();
}

void TrackModel::loadUserTracks(const QString &urn)
{
    m_lastKind = QStringLiteral("user-tracks");
    m_lastArg = urn;
    m_appendMode = false;
    setBusy(true);
    setError(QString());
    QString id = urn;
    const int c = id.lastIndexOf(QLatin1Char(':'));
    if (c >= 0)
        id = id.mid(c + 1);
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 50);
    q.insert(QStringLiteral("linked_partitioning"), true);
    m_api->get(QStringLiteral("/users/%1/tracks").arg(id), q, tagFor(QStringLiteral("list")));
}

void TrackModel::loadLikes()
{
    m_lastKind = QStringLiteral("likes");
    m_lastArg.clear();
    m_appendMode = false;
    beginLoadAll();
    setBusy(true);
    setError(QString());
    // api-v2: /me/likes 404s — get /me then /users/{id}/likes
    m_api->get(QStringLiteral("/me"), QVariantMap(), tagFor(QStringLiteral("me")));
}

void TrackModel::loadHistory()
{
    m_lastKind = QStringLiteral("history");
    m_lastQuery.clear();
    m_lastArg.clear();
    m_appendMode = false;
    setBusy(true);
    setError(QString());
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 25);
    // play history (not /stream/users/me which is the activity feed)
    m_api->get(QStringLiteral("/me/play_history/tracks"), q, tagFor(QStringLiteral("list")));
}

void TrackModel::loadPlaylistTracks(const QString &urn)
{
    m_lastKind = QStringLiteral("playlist-tracks");
    m_lastArg = urn;
    m_appendMode = false;
    beginLoadAll();
    setBusy(true);
    setError(QString());
    m_plOrder.clear();
    m_plHydrateQueue.clear();
    m_plHydrated.clear();
    QString id = urn;
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 200);
    q.insert(QStringLiteral("linked_partitioning"), 1);
    // /playlists/{id}/tracks was removed (404) — the object endpoint with
    // representation=full embeds every track as a full object (remaining
    // entries are id-only stubs hydrated via /tracks?ids=).
    q.insert(QStringLiteral("representation"), QStringLiteral("full"));

    if (id.contains(QLatin1String("system-playlists"))
            || id.contains(QLatin1String("-stations:"))
            || id.startsWith(QLatin1String("charts-"))) {
        // mixes / stations / charts: URN-addressed system playlist endpoint.
        // Only the full URN form is accepted (short form → 400).
        if (!id.contains(QLatin1String("system-playlists"))) {
            if (id.startsWith(QLatin1String("soundcloud:")))
                id = id.mid(11);
            id = QStringLiteral("soundcloud:system-playlists:") + id;
        } else if (!id.startsWith(QLatin1String("soundcloud:"))) {
            id.prepend(QStringLiteral("soundcloud:"));
        }
        m_api->get(QStringLiteral("/system-playlists/") + id, q, tagFor(QStringLiteral("pl")));
        return;
    }
    const int c = id.lastIndexOf(QLatin1Char(':'));
    if (c >= 0)
        id = id.mid(c + 1);
    m_api->get(QStringLiteral("/playlists/%1").arg(id), q, tagFor(QStringLiteral("pl")));
}

void TrackModel::loadRelated(const QString &urn)
{
    m_lastKind = QStringLiteral("related");
    m_lastArg = urn;
    m_appendMode = false;
    setBusy(true);
    setError(QString());
    QString id = urn;
    const int c = id.lastIndexOf(QLatin1Char(':'));
    if (c >= 0)
        id = id.mid(c + 1);
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 20);
    m_api->get(QStringLiteral("/tracks/%1/related").arg(id), q, tagFor(QStringLiteral("list")));
}

void TrackModel::loadFeed()
{
    m_lastKind = QStringLiteral("feed");
    m_lastArg.clear();
    m_appendMode = false;
    setBusy(true);
    setError(QString());
    QVariantMap q;
    q.insert(QStringLiteral("limit"), 50);
    q.insert(QStringLiteral("linked_partitioning"), true);
    m_api->get(QStringLiteral("/me/feed"), q, tagFor(QStringLiteral("list")));
}

void TrackModel::setItems(const QVariantList &items)
{
    setBusy(false);
    setError(QString());
    QVariantList norm;
    for (const QVariant &v : items) {
        const QVariantMap raw = v.toMap();
        if (raw.isEmpty())
            continue;
        // normalise api-v2 track objects
        if (raw.contains(QStringLiteral("media")) || raw.contains(QStringLiteral("duration"))
                || raw.contains(QStringLiteral("permalink_url")) || raw.contains(QStringLiteral("title")))
            norm.append(JsonParser::trackMap(QJsonObject::fromVariantMap(raw)));
        else
            norm.append(raw);
    }
    resetTo(norm, QString());
}

void TrackModel::refresh()
{
    reload();
}

void TrackModel::reload()
{
    if (m_lastKind == QLatin1String("search"))
        search(m_lastQuery);
    else if (m_lastKind == QLatin1String("home"))
        loadHome();
    else if (m_lastKind == QLatin1String("charts"))
        loadCharts();
    else if (m_lastKind == QLatin1String("user-tracks"))
        loadUserTracks(m_lastArg);
    else if (m_lastKind == QLatin1String("likes"))
        loadLikes();
    else if (m_lastKind == QLatin1String("history"))
        loadHistory();
    else if (m_lastKind == QLatin1String("playlist-tracks"))
        loadPlaylistTracks(m_lastArg);
    else if (m_lastKind == QLatin1String("related"))
        loadRelated(m_lastArg);
    else if (m_lastKind == QLatin1String("feed"))
        loadFeed();
}

void TrackModel::loadMore()
{
    if (m_nextHref.isEmpty() || m_busy)
        return;
    m_appendMode = true;
    setBusy(true);
    // continue the same chain this model was loading
    const QString tag = m_pageTag.isEmpty() ? tagFor(QStringLiteral("list")) : m_pageTag;
    m_api->getMore(m_nextHref, tag);
}

QVariantMap TrackModel::get(int row) const
{
    if (row < 0 || row >= m_items.size())
        return QVariantMap();
    return m_items.at(row).toMap();
}

void TrackModel::setFavorite(const QString &urn, bool favorite)
{
    for (int i = 0; i < m_items.size(); ++i) {
        QVariantMap m = m_items.at(i).toMap();
        if (m.value(QStringLiteral("urn")).toString() == urn) {
            m.insert(QStringLiteral("userFavorite"), favorite);
            m_items[i] = m;
            const QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx);
            break;
        }
    }
}
