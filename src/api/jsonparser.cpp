#include "jsonparser.h"

#include <QJsonArray>
#include <QJsonValue>

namespace JsonParser {

ScUser parseUser(const QJsonObject &o)
{
    ScUser u;
    u.id = qint64(o.value(QStringLiteral("id")).toDouble());
    u.urn = o.value(QStringLiteral("urn")).toString();
    u.username = o.value(QStringLiteral("username")).toString();
    u.fullName = o.value(QStringLiteral("full_name")).toString();
    u.city = o.value(QStringLiteral("city")).toString();
    u.country = o.value(QStringLiteral("country")).toString();
    u.avatarUrl = o.value(QStringLiteral("avatar_url")).toString();
    u.permalinkUrl = o.value(QStringLiteral("permalink_url")).toString();
    u.followersCount = o.value(QStringLiteral("followers_count")).toInt();
    u.followingsCount = o.value(QStringLiteral("followings_count")).toInt();
    u.trackCount = o.value(QStringLiteral("track_count")).toInt();
    u.playlistCount = o.value(QStringLiteral("playlist_count")).toInt();
    u.description = o.value(QStringLiteral("description")).toString();
    return u;
}

ScTrack parseTrack(const QJsonObject &o)
{
    ScTrack t;
    t.id = qint64(o.value(QStringLiteral("id")).toDouble());
    t.urn = o.value(QStringLiteral("urn")).toString();
    if (t.urn.isEmpty())
        t.urn = QStringLiteral("soundcloud:tracks:%1").arg(t.id);
    t.title = o.value(QStringLiteral("title")).toString();
    t.description = o.value(QStringLiteral("description")).toString();
    t.genre = o.value(QStringLiteral("genre")).toString();

    // artwork: artwork_url → user.avatar_url (yt-dlp fallback)
    QString art = o.value(QStringLiteral("artwork_url")).toString();
    if (art.isEmpty())
        art = o.value(QStringLiteral("artworkUrl")).toString();
    if (art.isEmpty())
        art = o.value(QStringLiteral("user")).toObject().value(QStringLiteral("avatar_url")).toString();
    t.artworkUrl = art;

    t.permalinkUrl = o.value(QStringLiteral("permalink_url")).toString();
    if (t.permalinkUrl.isEmpty())
        t.permalinkUrl = o.value(QStringLiteral("permalinkUrl")).toString();
    t.streamUrl = o.value(QStringLiteral("stream_url")).toString();
    t.waveformUrl = o.value(QStringLiteral("waveform_url")).toString();
    t.access = o.value(QStringLiteral("access")).toString();
    if (t.access.isEmpty()) {
        if (o.value(QStringLiteral("snipped")).toBool())
            t.access = QStringLiteral("preview");
        else
            t.access = o.value(QStringLiteral("streamable")).toBool(true)
                    ? QStringLiteral("playable") : QStringLiteral("blocked");
    }
    // duration is milliseconds on api-v2
    t.durationMs = qint64(o.value(QStringLiteral("duration")).toDouble());
    if (t.durationMs == 0)
        t.durationMs = qint64(o.value(QStringLiteral("durationMs")).toDouble());
    t.playbackCount = qint64(o.value(QStringLiteral("playback_count")).toDouble());
    t.likesCount = qint64(o.value(QStringLiteral("likes_count")).toDouble());
    if (t.likesCount == 0)
        t.likesCount = qint64(o.value(QStringLiteral("favoritings_count")).toDouble());
    t.commentCount = qint64(o.value(QStringLiteral("comment_count")).toDouble());
    t.userFavorite = o.value(QStringLiteral("user_favorite")).toBool();
    if (!t.userFavorite)
        t.userFavorite = o.value(QStringLiteral("userFavorite")).toBool();
    t.streamable = o.value(QStringLiteral("streamable")).toBool(true);
    t.user = parseUser(o.value(QStringLiteral("user")).toObject());
    return t;
}

ScPlaylist parsePlaylist(const QJsonObject &o)
{
    ScPlaylist p;
    p.id = qint64(o.value(QStringLiteral("id")).toDouble());
    p.urn = o.value(QStringLiteral("urn")).toString();
    if (p.urn.isEmpty())
        p.urn = QStringLiteral("soundcloud:playlists:%1").arg(p.id);
    p.title = o.value(QStringLiteral("title")).toString();
    p.description = o.value(QStringLiteral("description")).toString();
    p.artworkUrl = o.value(QStringLiteral("artwork_url")).toString();
    p.permalinkUrl = o.value(QStringLiteral("permalink_url")).toString();
    p.trackCount = o.value(QStringLiteral("track_count")).toInt();
    p.durationMs = qint64(o.value(QStringLiteral("duration")).toDouble());
    p.kind = o.value(QStringLiteral("kind")).toString();
    if (p.kind.isEmpty())
        p.kind = QStringLiteral("playlist");
    p.user = parseUser(o.value(QStringLiteral("user")).toObject());
    const QJsonArray tracks = o.value(QStringLiteral("tracks")).toArray();
    for (const QJsonValue &v : tracks) {
        if (v.isObject())
            p.tracks.append(parseTrack(v.toObject()));
    }
    return p;
}

ScStreamUrls parseStreams(const QJsonObject &o)
{
    ScStreamUrls s;
    s.hlsAac160 = o.value(QStringLiteral("hls_aac_160_url")).toString();
    s.hlsMp3128 = o.value(QStringLiteral("hls_mp3_128_url")).toString();
    s.previewMp3128 = o.value(QStringLiteral("preview_mp3_128_url")).toString();
    // progressive fields vary; scan keys
    const QStringList progressiveKeys = {
        QStringLiteral("http_mp3_128_url"),
        QStringLiteral("progressive_mp3_128_url"),
        QStringLiteral("mp3_128_url")
    };
    for (const QString &k : progressiveKeys) {
        const QString v = o.value(k).toString();
        if (!v.isEmpty()) {
            s.progressiveMp3 = v;
            break;
        }
    }

    // api-v2: media.transcodings[].url + preset
    const QJsonArray trans = o.value(QStringLiteral("media")).toObject()
            .value(QStringLiteral("transcodings")).toArray();
    for (const QJsonValue &v : trans) {
        const QJsonObject t = v.toObject();
        const QString url = t.value(QStringLiteral("url")).toString();
        const QString preset = t.value(QStringLiteral("preset")).toString();
        const QString protocol = t.value(QStringLiteral("format")).toObject()
                .value(QStringLiteral("protocol")).toString();
        if (url.isEmpty())
            continue;
        if (preset.contains(QLatin1String("hls_aac"))) {
            if (s.hlsAac160.isEmpty())
                s.hlsAac160 = url;
        } else if (preset.contains(QLatin1String("hls_mp3"))) {
            if (s.hlsMp3128.isEmpty())
                s.hlsMp3128 = url;
        } else if (preset.contains(QLatin1String("preview"))) {
            if (s.previewMp3128.isEmpty())
                s.previewMp3128 = url;
        } else if (protocol == QLatin1String("http") || preset.contains(QLatin1String("mp3"))) {
            if (s.progressiveMp3.isEmpty())
                s.progressiveMp3 = url;
        }
    }
    return s;
}

QVariantList parseCollection(const QJsonObject &root)
{
    QVariantList out;
    const QJsonValue colVal = root.value(QStringLiteral("collection"));
    if (!colVal.isArray())
        return out;
    const QJsonArray arr = colVal.toArray();
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        QJsonObject o = v.toObject();

        // mixed-selections: items is { collection: [...] }
        const QJsonValue itemsVal = o.value(QStringLiteral("items"));
        QJsonArray itemArr;
        if (itemsVal.isArray()) {
            itemArr = itemsVal.toArray();
        } else if (itemsVal.isObject()) {
            const QJsonValue c = itemsVal.toObject().value(QStringLiteral("collection"));
            if (c.isArray())
                itemArr = c.toArray();
        }
        if (!itemArr.isEmpty()) {
            for (const QJsonValue &iv : itemArr) {
                if (!iv.isObject())
                    continue;
                QJsonObject io = iv.toObject();
                if (io.value(QStringLiteral("playlist")).isObject())
                    io = io.value(QStringLiteral("playlist")).toObject();
                if (io.value(QStringLiteral("track")).isObject())
                    io = io.value(QStringLiteral("track")).toObject();
                const QString ikind = io.value(QStringLiteral("kind")).toString();
                if (ikind == QLatin1String("playlist") || ikind == QLatin1String("system-playlist")
                        || io.contains(QStringLiteral("track_count")))
                    out.append(parsePlaylist(io).toVariantMap());
                else if (ikind == QLatin1String("track") || io.contains(QStringLiteral("duration"))
                         || io.contains(QStringLiteral("media")))
                    out.append(parseTrack(io).toVariantMap());
            }
            continue;
        }

        if (o.value(QStringLiteral("track")).isObject())
            o = o.value(QStringLiteral("track")).toObject();
        if (o.value(QStringLiteral("playlist")).isObject() && !o.contains(QStringLiteral("title")))
            o = o.value(QStringLiteral("playlist")).toObject();
        const QString kind = o.value(QStringLiteral("kind")).toString();
        // kind must win over loose heuristics — playlists also have permalink_url/artwork_url
        if (kind == QLatin1String("playlist") || kind == QLatin1String("system-playlist")
                || o.contains(QStringLiteral("track_count")))
            out.append(parsePlaylist(o).toVariantMap());
        else if (kind == QLatin1String("track") || o.contains(QStringLiteral("media"))
                || o.contains(QStringLiteral("permalink_url")) || o.contains(QStringLiteral("artwork_url")))
            out.append(parseTrack(o).toVariantMap());
        else if (kind == QLatin1String("user"))
            out.append(parseUser(o).toVariantMap());
    }
    return out;
}

QVariantMap trackMap(const QJsonObject &o) { return parseTrack(o).toVariantMap(); }
QVariantMap userMap(const QJsonObject &o) { return parseUser(o).toVariantMap(); }
QVariantMap playlistMap(const QJsonObject &o) { return parsePlaylist(o).toVariantMap(); }

} // namespace JsonParser
