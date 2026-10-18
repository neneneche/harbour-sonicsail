#pragma once

#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>

struct ScUser {
    qint64 id = 0;
    QString urn;
    QString username;
    QString fullName;
    QString city;
    QString country;
    QString avatarUrl;
    QString permalinkUrl;
    int followersCount = 0;
    int followingsCount = 0;
    int trackCount = 0;
    int playlistCount = 0;
    QString description;

    QVariantMap toVariantMap() const {
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("urn"), urn);
        m.insert(QStringLiteral("username"), username);
        m.insert(QStringLiteral("fullName"), fullName);
        m.insert(QStringLiteral("city"), city);
        m.insert(QStringLiteral("country"), country);
        m.insert(QStringLiteral("avatarUrl"), avatarUrl);
        m.insert(QStringLiteral("permalinkUrl"), permalinkUrl);
        m.insert(QStringLiteral("followersCount"), followersCount);
        m.insert(QStringLiteral("followingsCount"), followingsCount);
        m.insert(QStringLiteral("trackCount"), trackCount);
        m.insert(QStringLiteral("playlistCount"), playlistCount);
        m.insert(QStringLiteral("description"), description);
        return m;
    }
};

struct ScTrack {
    qint64 id = 0;
    QString urn;
    QString title;
    QString description;
    QString genre;
    QString artworkUrl;
    QString permalinkUrl;
    QString streamUrl;
    QString waveformUrl;
    QString access; // playable | preview | blocked
    qint64 durationMs = 0;
    qint64 playbackCount = 0;
    qint64 likesCount = 0;
    qint64 commentCount = 0;
    bool userFavorite = false;
    bool streamable = true;
    ScUser user;

    QVariantMap toVariantMap() const {
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("urn"), urn);
        m.insert(QStringLiteral("title"), title);
        m.insert(QStringLiteral("description"), description);
        m.insert(QStringLiteral("genre"), genre);
        m.insert(QStringLiteral("artworkUrl"), artworkUrl);
        m.insert(QStringLiteral("permalinkUrl"), permalinkUrl);
        m.insert(QStringLiteral("streamUrl"), streamUrl);
        m.insert(QStringLiteral("waveformUrl"), waveformUrl);
        m.insert(QStringLiteral("access"), access);
        m.insert(QStringLiteral("durationMs"), durationMs);
        m.insert(QStringLiteral("durationText"), durationText());
        m.insert(QStringLiteral("playbackCount"), playbackCount);
        m.insert(QStringLiteral("likesCount"), likesCount);
        m.insert(QStringLiteral("commentCount"), commentCount);
        m.insert(QStringLiteral("userFavorite"), userFavorite);
        m.insert(QStringLiteral("streamable"), streamable);
        m.insert(QStringLiteral("user"), user.toVariantMap());
        m.insert(QStringLiteral("username"), user.username);
        m.insert(QStringLiteral("playable"), access != QLatin1String("blocked"));
        m.insert(QStringLiteral("kind"), QStringLiteral("track"));
        return m;
    }

    QString durationText() const {
        const int totalSec = int(durationMs / 1000);
        const int m = totalSec / 60;
        const int s = totalSec % 60;
        return QStringLiteral("%1:%2")
                .arg(m)
                .arg(s, 2, 10, QLatin1Char('0'));
    }
};

struct ScPlaylist {
    qint64 id = 0;
    QString urn;
    QString title;
    QString description;
    QString artworkUrl;
    QString permalinkUrl;
    int trackCount = 0;
    qint64 durationMs = 0;
    QString kind; // "playlist" | "system-playlist"
    ScUser user;
    QList<ScTrack> tracks;

    QVariantMap toVariantMap() const {
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("urn"), urn);
        m.insert(QStringLiteral("title"), title);
        m.insert(QStringLiteral("description"), description);
        m.insert(QStringLiteral("artworkUrl"), artworkUrl);
        m.insert(QStringLiteral("permalinkUrl"), permalinkUrl);
        m.insert(QStringLiteral("trackCount"), trackCount);
        m.insert(QStringLiteral("durationMs"), durationMs);
        m.insert(QStringLiteral("user"), user.toVariantMap());
        m.insert(QStringLiteral("username"), user.username);
        m.insert(QStringLiteral("kind"),
                 kind.isEmpty() ? QStringLiteral("playlist") : kind);
        QVariantList tl;
        for (const ScTrack &t : tracks)
            tl.append(t.toVariantMap());
        m.insert(QStringLiteral("tracks"), tl);
        return m;
    }
};

struct ScStreamUrls {
    QString hlsAac160;
    QString hlsMp3128;
    QString previewMp3128;
    QString progressiveMp3;
    QString legacyStreamUrl;

    bool hasAny() const {
        return !hlsAac160.isEmpty() || !hlsMp3128.isEmpty()
                || !previewMp3128.isEmpty() || !progressiveMp3.isEmpty()
                || !legacyStreamUrl.isEmpty();
    }
};

struct ScPage {
    QVariantList items;
    QString nextHref;
    bool isEmpty() const { return items.isEmpty() && nextHref.isEmpty(); }
};
