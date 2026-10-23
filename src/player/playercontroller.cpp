#include "playercontroller.h"
#include "trackqueue.h"
#include "../api/apiclient.h"
#include "../api/jsonparser.h"
#include "../api/apitypes.h"

#include <QDebug>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

PlayerController::PlayerController(ApiClient *api, TrackQueue *queue, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_queue(queue)
    , m_player(new QMediaPlayer(this))
{
    qsrand(uint(QDateTime::currentMSecsSinceEpoch()));
    m_player->setVolume(100);
    connect(m_player, &QMediaPlayer::stateChanged, this, &PlayerController::onPlayerStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &PlayerController::onMediaStatusChanged);
    connect(m_player, static_cast<void(QMediaPlayer::*)(QMediaPlayer::Error)>(&QMediaPlayer::error),
            this, &PlayerController::onPlayerError);
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        m_position = int(pos);
        emit positionChanged();
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
        m_duration = int(dur);
        emit durationChanged();
    });

    connect(m_api, &ApiClient::success, this, &PlayerController::onStreamsReceived);
    connect(m_api, &ApiClient::success, this, &PlayerController::onRelatedReceived);
    connect(m_api, &ApiClient::failed, this, &PlayerController::onApiFailed);
    connect(m_queue, &TrackQueue::currentTrackChanged, this, &PlayerController::likedChanged);

    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, &PlayerController::onTick);
}

bool PlayerController::isPlaying() const
{
    return m_player->state() == QMediaPlayer::PlayingState;
}

void PlayerController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void PlayerController::setStatus(const QString &msg)
{
    if (m_statusMessage == msg)
        return;
    m_statusMessage = msg;
    emit statusMessageChanged();
}

void PlayerController::loadWaveform(const QVariantMap &track)
{
    m_waveformSamples.clear();
    emit waveformChanged();

    QString u = track.value(QStringLiteral("waveformUrl")).toString();
    if (u.isEmpty())
        u = track.value(QStringLiteral("waveform_url")).toString();
    if (u.isEmpty())
        return;
    // waveform png serves the same peaks as json; URLs may already be json
    if (u.endsWith(QLatin1String(".png"))) {
        u.chop(4);
        u += QStringLiteral(".json");
    } else {
        const int png = u.indexOf(QLatin1String(".png"));
        if (png > 0)
            u = u.left(png) + QStringLiteral(".json");
    }

    if (m_waveReply) {
        m_waveReply->disconnect(this);
        m_waveReply->abort();
        m_waveReply = nullptr;
    }
    QNetworkRequest req{QUrl(u)};
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    req.setRawHeader("Referer", "https://soundcloud.com/");
    req.setRawHeader("Accept", "application/json");
    m_waveReply = m_waveNam.get(req);
    connect(m_waveReply, &QNetworkReply::finished, this, &PlayerController::onWaveformReply);
}

void PlayerController::onWaveformReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply == m_waveReply)
        m_waveReply = nullptr;
    if (reply->error() != QNetworkReply::NoError)
        return;

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    QJsonArray samples = obj.value(QStringLiteral("samples")).toArray();
    if (samples.isEmpty())
        samples = obj.value(QStringLiteral("peaks")).toArray();
    if (samples.isEmpty())
        return;

    QVariantList out;
    out.reserve(samples.size());
    for (const QJsonValue &v : samples)
        out.append(v.toDouble());
    m_waveformSamples = out;
    emit waveformChanged();
}

void PlayerController::updateMetadata()
{
    const QVariantMap t = m_queue->currentTrack();
    m_title = t.value(QStringLiteral("title")).toString();
    m_artist = t.value(QStringLiteral("username")).toString();
    if (m_artist.isEmpty())
        m_artist = t.value(QStringLiteral("user")).toMap().value(QStringLiteral("username")).toString();
    m_artworkUrl = t.value(QStringLiteral("artworkUrl")).toString();
    m_permalinkUrl = t.value(QStringLiteral("permalinkUrl")).toString();
    m_currentUrn = t.value(QStringLiteral("urn")).toString();
    m_reported = false;
    // waveform peaks fetch — commented out with the feature (not working yet)
    // loadWaveform(t);
    emit trackChanged();
    emit likedChanged();
}

void PlayerController::playIndex(int index)
{
    m_queue->setCurrentIndex(index);
    loadCurrent();
}

void PlayerController::playTrack(const QVariantMap &track)
{
    // Radio-style: one track now, related tracks fill the rest of the queue
    startRadio(track);
}

void PlayerController::playQueue(const QVariantList &tracks, int startIndex)
{
    m_queue->setFromList(tracks, startIndex);
    loadCurrent();
}

void PlayerController::shuffleQueue()
{
    // Fisher–Yates around current index
    const int n = m_queue->rowCount();
    if (n < 3)
        return;
    QList<int> order;
    for (int i = 0; i < n; ++i)
        if (i != m_queue->currentIndex())
            order.append(i);
    for (int i = order.size() - 1; i > 0; --i) {
        const int j = qrand() % (i + 1);
        order.swap(i, j);
    }
    QList<int> seq;
    seq.append(m_queue->currentIndex());
    seq.append(order);
    // rebuild model via setFromList of shuffled get()
    QVariantList shuffled;
    for (int idx : seq)
        shuffled.append(m_queue->get(idx));
    m_queue->setFromList(shuffled, 0);
    if (m_shuffle)
        emit shuffleChanged();
}

void PlayerController::toggleLike(const QVariantMap &track)
{
    const bool liked = track.value(QStringLiteral("userFavorite")).toBool();
    const QString urn = track.value(QStringLiteral("urn")).toString();
    // optimistic UI update only — the network write goes through the
    // WebWriter hidden-browser bridge (bot protection rejects plain HTTP)
    m_queue->setFavorite(urn, !liked);
    emit likedChanged();
}

bool PlayerController::liked() const
{
    return m_queue->currentTrack().value(QStringLiteral("userFavorite")).toBool();
}

void PlayerController::toggleCurrentLike()
{
    QVariantMap t = m_queue->currentTrack();
    if (t.isEmpty())
        return;
    toggleLike(t);
}

QVariantMap PlayerController::currentTrack() const
{
    return m_queue ? m_queue->currentTrack() : QVariantMap();
}

void PlayerController::likeTrack(const QString &urn, bool like)
{
    Q_UNUSED(urn)
    Q_UNUSED(like)
    // network writes are performed by the WebWriter bridge from QML
}

void PlayerController::startRadio(const QVariantMap &track)
{
    m_queue->clear();
    m_queue->append(track);
    m_queue->setCurrentIndex(0);
    loadCurrent();

    // fetch related → append as radio
    QString id = track.value(QStringLiteral("id")).toString();
    if (id.isEmpty() || id == QLatin1String("0"))
        id = QString::number(qint64(track.value(QStringLiteral("id")).toDouble()));
    if (id.isEmpty() || id == QLatin1String("0")) {
        QString urn = track.value(QStringLiteral("urn")).toString();
        const int c = urn.lastIndexOf(QLatin1Char(':'));
        if (c >= 0)
            id = urn.mid(c + 1);
    }
    if (!id.isEmpty() && id != QLatin1String("0"))
        m_api->get(QStringLiteral("/tracks/%1/related").arg(id),
                   QVariantMap{{QStringLiteral("limit"), 25}},
                   QStringLiteral("player-related"));
}

void PlayerController::onRelatedReceived(const QString &tag, const QVariantMap &data)
{
    if (tag != QLatin1String("player-related"))
        return;
    const QVariantList tracks = data.value(QStringLiteral("collection")).toList();
    for (const QVariant &tv : tracks) {
        QVariantMap raw = tv.toMap();
        if (raw.contains(QStringLiteral("track")))
            raw = raw.value(QStringLiteral("track")).toMap();
        if (raw.isEmpty())
            continue;
        m_queue->append(JsonParser::trackMap(QJsonObject::fromVariantMap(raw)));
    }
    if (m_shuffle)
        shuffleQueue();
}

void PlayerController::playFromModel(QObject *model, int index)
{
    // Extract all rows from a TrackModel-like QAbstractListModel
    QAbstractListModel *lm = qobject_cast<QAbstractListModel *>(model);
    if (!lm) {
        playIndex(index);
        return;
    }
    QVariantList tracks;
    const int n = lm->rowCount();
    for (int i = 0; i < n; ++i) {
        const QModelIndex idx = lm->index(i, 0);
        QVariantMap m;
        const QHash<int, QByteArray> roles = lm->roleNames();
        for (auto it = roles.constBegin(); it != roles.constEnd(); ++it)
            m.insert(QString::fromUtf8(it.value()), lm->data(idx, it.key()));
        tracks.append(m);
    }
    if (tracks.isEmpty())
        return;
    playQueue(tracks, qBound(0, index, tracks.size() - 1));
}

void PlayerController::loadCurrent()
{
    // clear first so stale handlers cannot play the old track
    m_candidates.clear();
    m_transcodingUrls.clear();
    m_candidateIndex = -1;
    m_pendingPlay = false;
    ++m_loadGen;
    m_tick.stop();

    updateMetadata();
    m_player->stop();
    m_position = 0;
    m_duration = 0;
    emit positionChanged();
    emit durationChanged();

    const QVariantMap t = m_queue->currentTrack();
    QString idPart = t.value(QStringLiteral("id")).toString();
    if (idPart.isEmpty() || idPart == QLatin1String("0")) {
        QString urn = t.value(QStringLiteral("urn")).toString();
        const int colon = urn.lastIndexOf(QLatin1Char(':'));
        if (colon >= 0)
            idPart = urn.mid(colon + 1);
    }
    if (idPart.isEmpty()) {
        setStatus(tr("Nothing to play"));
        return;
    }

    setBusy(true);
    setStatus(tr("Loading stream…"));
    m_api->get(QStringLiteral("/tracks/%1").arg(idPart), QVariantMap(),
               QStringLiteral("player-track"));
}

void PlayerController::resolveStreams(const QString &urn)
{
    Q_UNUSED(urn)
}

void PlayerController::onStreamsReceived(const QString &tag, const QVariantMap &data)
{
    if (tag == QLatin1String("player-track")) {
        setBusy(false);
        m_candidates.clear();
        m_candidateIndex = -1;

        // Build ordered list of api-v2 transcoding URLs (need resolve step)
        const QVariantList trans = data.value(QStringLiteral("media")).toMap()
                .value(QStringLiteral("transcodings")).toList();

        struct Cand { QString url; int rank; };
        QList<Cand> cands;

        for (const QVariant &tv : trans) {
            const QVariantMap t = tv.toMap();
            const QString url = t.value(QStringLiteral("url")).toString();
            const QString preset = t.value(QStringLiteral("preset")).toString();
            const QString protocol = t.value(QStringLiteral("format")).toMap()
                    .value(QStringLiteral("protocol")).toString();
            if (url.isEmpty())
                continue;
            if (preset.contains(QLatin1String("ctr")) || preset.contains(QLatin1String("cbc")))
                continue; // DRM

            // rank: http_aac=0, hls_aac=1, http_mp3=2, hls_mp3=3, preview=9
            int rank = 5;
            const bool progressive = (protocol == QLatin1String("progressive")
                                      || protocol == QLatin1String("http")
                                      || url.contains(QLatin1String("/stream/progressive")));
            const bool isMp3 = preset.contains(QLatin1String("mp3"));
            const bool isAac = preset.contains(QLatin1String("aac"));
            const bool isPreview = preset.contains(QLatin1String("preview"))
                    || url.contains(QLatin1String("/preview/"))
                    || t.value(QStringLiteral("snipped")).toBool();

            if (isPreview)
                rank = 9;
            else if (progressive && isMp3)
                rank = 0;
            else if (progressive && isAac)
                rank = 1;
            else if (!progressive && isAac)
                rank = 2; // HLS AAC — SoundCloud default in 2026
            else if (!progressive && isMp3)
                rank = 3;
            else
                rank = 5;

            cands.append({url, rank});
        }

        std::sort(cands.begin(), cands.end(),
                  [](const Cand &a, const Cand &b) { return a.rank < b.rank; });

        for (const Cand &c : cands)
            m_candidates.append(c.url);

        // Keep preset/protocol info alongside for resolve failures
        m_transcodingUrls = m_candidates;

        if (m_candidates.isEmpty()) {
            handlePlaybackFailure(tr("No playable stream"));
            return;
        }

        setBusy(true);
        setStatus(tr("Resolving stream…"));
        m_candidateIndex = 0;
        {
            const QString first = m_candidates.at(0);
            // already a direct CDN file — play now (no second resolve)
            if (first.contains(QLatin1String("cf-media")) || first.contains(QLatin1String("sndcdn"))) {
                playResolved(QUrl(first));
                return;
            }
            m_api->get(first, QVariantMap(), QStringLiteral("player-media"));
        }
        return;
    }

    if (tag == QLatin1String("player-media")) {
        setBusy(false);
        const QString realUrl = data.value(QStringLiteral("url")).toString();
        if (!realUrl.isEmpty()) {
            playResolved(QUrl(realUrl));
            return;
        }
        // try next transcoding
        if (m_candidateIndex + 1 < m_candidates.size()) {
            ++m_candidateIndex;
            setBusy(true);
            m_api->get(m_candidates.at(m_candidateIndex), QVariantMap(),
                       QStringLiteral("player-media"));
            return;
        }
        handlePlaybackFailure(tr("No playable stream"));
        return;
    }
}

void PlayerController::tryPlay(const QUrl &url)
{
    Q_UNUSED(url)
    // replaced by playResolved below
}

void PlayerController::playResolved(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty()) {
        handlePlaybackFailure(tr("Could not resolve stream"));
        return;
    }

    const QString s = url.toString();
    qWarning() << "Player: setMedia" << s.left(120);

    // SoundCloud is HLS-only for many tracks (aac_160k / abr_sq m3u8).
    // gstreamer adaptivedemux2 on SFOS can play these.
    m_player->setMuted(false);
    m_player->setVolume(100);
    m_player->setMedia(QMediaContent(url));
    m_player->play();
    m_tick.start();
    m_pendingPlay = true;

    QTimer::singleShot(2000, this, [this]() {
        if (m_pendingPlay && m_player->state() != QMediaPlayer::PlayingState) {
            qWarning() << "Player: retry play, status" << int(m_player->mediaStatus());
            m_player->play();
        }
    });
}

void PlayerController::onPlayerStateChanged(QMediaPlayer::State state)
{
    if (state == QMediaPlayer::PlayingState) {
        setStatus(QString());
        m_failStreak = 0;
        reportPlayed();
    }
    emit stateChanged();
}

void PlayerController::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_repeatMode == 2) {
            loadCurrent();
            return;
        }
        if (m_queue->hasNext() || m_repeatMode == 1)
            next();
        else
            stop();
        return;
    }
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
        if (m_pendingPlay) {
            m_pendingPlay = false;
            m_player->play();
            m_tick.start();
        }
        setStatus(QString());
        return;
    }
    // Only InvalidMedia is a failure — UnknownMediaStatus is the initial state
    if (status == QMediaPlayer::InvalidMedia) {
        if (m_candidateIndex + 1 < m_candidates.size()) {
            ++m_candidateIndex;
            qWarning() << "Player: invalid media, try candidate" << m_candidateIndex;
            setBusy(true);
            m_api->get(m_candidates.at(m_candidateIndex), QVariantMap(),
                       QStringLiteral("player-media"));
            return;
        }
        handlePlaybackFailure(tr("Playback failed"));
    }
}

void PlayerController::onPlayerError(QMediaPlayer::Error error)
{
    qWarning() << "Player: error" << int(error) << "candidate" << m_candidateIndex;
    if (m_candidateIndex + 1 < m_candidates.size()) {
        ++m_candidateIndex;
        setBusy(true);
        m_api->get(m_candidates.at(m_candidateIndex), QVariantMap(),
                   QStringLiteral("player-media"));
        return;
    }
    handlePlaybackFailure(tr("Playback failed"));
}

void PlayerController::onApiFailed(const QString &tag, int status, const QString &message)
{
    if (!tag.startsWith(QLatin1String("player")))
        return;

    setBusy(false);

    if (tag == QLatin1String("player-media")
            && m_candidateIndex + 1 < m_candidates.size()) {
        ++m_candidateIndex;
        setBusy(true);
        m_api->get(m_candidates.at(m_candidateIndex), QVariantMap(),
                   QStringLiteral("player-media"));
        return;
    }

    if (tag == QLatin1String("player-track")) {
        handlePlaybackFailure(message.isEmpty() ? tr("Could not load track") : message);
        return;
    }
    handlePlaybackFailure(message.isEmpty() ? tr("Playback failed") : message);
    Q_UNUSED(status)
}

void PlayerController::toggle()
{
    if (isPlaying())
        pause();
    else
        play();
}

void PlayerController::play()
{
    if (m_player->media().isNull() && m_queue->currentIndex() >= 0) {
        loadCurrent();
        return;
    }
    m_player->play();
    m_tick.start();
}

void PlayerController::pause()
{
    m_player->pause();
}

void PlayerController::stop()
{
    m_player->stop();
    m_tick.stop();
    m_position = 0;
    emit positionChanged();
    emit stateChanged();
}

void PlayerController::setShuffle(bool on)
{
    if (m_shuffle == on)
        return;
    m_shuffle = on;
    if (on && m_queue->rowCount() > 1)
        shuffleQueue();
    emit shuffleChanged();
}

void PlayerController::setRepeatMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (m_repeatMode == mode)
        return;
    m_repeatMode = mode;
    emit repeatModeChanged();
}

void PlayerController::next()
{
    if (m_repeatMode == 2) { // repeat one
        loadCurrent();
        return;
    }
    // shuffle already reorders the queue — play sequentially through it
    if (m_queue->hasNext()) {
        playIndex(m_queue->currentIndex() + 1);
    } else if (m_repeatMode == 1 && m_queue->rowCount() > 0) {
        if (m_shuffle)
            shuffleQueue();
        playIndex(0);
    } else {
        stop();
    }
}

void PlayerController::previous()
{
    // restart if more than 3s in
    if (m_position > 3000) {
        seek(0);
        return;
    }
    if (m_queue->hasPrevious())
        playIndex(m_queue->currentIndex() - 1);
    else
        seek(0);
}

void PlayerController::seek(int positionMs)
{
    m_player->setPosition(positionMs);
    m_position = positionMs;
    emit positionChanged();
}

void PlayerController::onTick()
{
    m_position = int(m_player->position());
    emit positionChanged();
}

void PlayerController::reportPlayed()
{
    if (m_reported || m_currentUrn.isEmpty())
        return;
    m_reported = true;
    // best-effort recently-played tracking
    QString idPart = m_currentUrn;
    const int colon = idPart.lastIndexOf(QLatin1Char(':'));
    if (colon >= 0)
        idPart = idPart.mid(colon + 1);
    m_api->postSilent(QStringLiteral("/me/recently-played/%1").arg(idPart));
}

void PlayerController::handlePlaybackFailure(const QString &message)
{
    setStatus(message);
    // Cap consecutive failures so a dead queue / repeat-one does not spin forever.
    const int maxFails = qMax(3, m_queue->rowCount());
    if (++m_failStreak >= maxFails) {
        m_failStreak = 0;
        emit errorOccurred(message);
        stop();
        return;
    }
    // Advance via playIndex — never next(): next() reloads current on repeat-one
    if (m_queue->hasNext()) {
        const int go = m_queue->currentIndex() + 1;
        QTimer::singleShot(0, this, [this, go]() { playIndex(go); });
    } else if (m_repeatMode == 1 && m_queue->rowCount() > 0) {
        QTimer::singleShot(0, this, [this]() {
            if (m_shuffle)
                shuffleQueue();
            playIndex(0);
        });
    } else {
        m_failStreak = 0;
        emit errorOccurred(message);
        stop();
    }
}
