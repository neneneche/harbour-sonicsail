#include "downloadmanager.h"
#include "../api/apiclient.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

DownloadManager::DownloadManager(ApiClient *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
    connect(m_api, &ApiClient::success, this, &DownloadManager::onApiSuccess);
    connect(m_api, &ApiClient::failed, this, &DownloadManager::onApiFailed);
}

QString DownloadManager::musicDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    const QString dir = base + QStringLiteral("/Soundsail");
    QDir().mkpath(dir);
    return dir;
}

void DownloadManager::setStatus(const QString &s)
{
    if (m_status == s)
        return;
    m_status = s;
    emit statusMessageChanged();
}

void DownloadManager::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged();
}

void DownloadManager::downloadTrack(const QVariantMap &track, bool saveToMusic)
{
    QString title = track.value(QStringLiteral("title")).toString();
    QString artist = track.value(QStringLiteral("username")).toString();
    if (artist.isEmpty())
        artist = track.value(QStringLiteral("user")).toMap().value(QStringLiteral("username")).toString();
    if (title.isEmpty())
        title = QStringLiteral("track");

    QString safe = (artist + QStringLiteral(" - ") + title);
    safe.replace(QLatin1Char('/'), QLatin1Char('-'));
    safe.replace(QLatin1Char('\\'), QLatin1Char('-'));
    safe.replace(QLatin1Char(':'), QLatin1Char('-'));
    safe = safe.left(120);

    QString urn = track.value(QStringLiteral("urn")).toString();
    QString id = track.value(QStringLiteral("id")).toString();
    if (id.isEmpty() || id == QLatin1String("0")) {
        const int c = urn.lastIndexOf(QLatin1Char(':'));
        if (c >= 0)
            id = urn.mid(c + 1);
    }
    if (id.isEmpty()) {
        emit failed(tr("No track id"));
        return;
    }
    // manual download: allow re-download
    m_doneIds.remove(id);
    enqueue({id, safe, saveToMusic});
}

void DownloadManager::autoDownload(const QVariantMap &track, bool saveToMusic)
{
    QString urn = track.value(QStringLiteral("urn")).toString();
    QString id = track.value(QStringLiteral("id")).toString();
    if (id.isEmpty() || id == QLatin1String("0")) {
        const int c = urn.lastIndexOf(QLatin1Char(':'));
        if (c >= 0)
            id = urn.mid(c + 1);
    }
    if (id.isEmpty() || m_doneIds.contains(id) || m_activeIds.contains(id))
        return;

    QString title = track.value(QStringLiteral("title")).toString();
    QString artist = track.value(QStringLiteral("username")).toString();
    if (artist.isEmpty())
        artist = track.value(QStringLiteral("user")).toMap().value(QStringLiteral("username")).toString();
    if (title.isEmpty())
        title = QStringLiteral("track");
    QString safe = (artist + QStringLiteral(" - ") + title);
    safe.replace(QLatin1Char('/'), QLatin1Char('-'));
    safe.replace(QLatin1Char('\\'), QLatin1Char('-'));
    safe.replace(QLatin1Char(':'), QLatin1Char('-'));
    safe = safe.left(120);

    enqueue({id, safe, saveToMusic});
}

void DownloadManager::enqueue(const Job &job)
{
    m_queue.append(job);
    if (!m_busy)
        processNext();
}

void DownloadManager::processNext()
{
    if (m_busy || m_queue.isEmpty()) {
        if (!m_busy)
            setStatus(QString());
        return;
    }
    const Job job = m_queue.takeFirst();
    if (m_doneIds.contains(job.id) || m_activeIds.contains(job.id)) {
        processNext();
        return;
    }
    m_activeIds.insert(job.id);
    m_currentId = job.id;
    resolveAndDownload(job.id, job.baseName, job.toMusic);
}

void DownloadManager::finishJob(bool ok, const QString &message)
{
    if (!m_currentId.isEmpty()) {
        m_activeIds.remove(m_currentId);
        if (ok)
            m_doneIds.insert(m_currentId);
        m_currentId.clear();
    }
    setBusy(false);
    if (!ok) {
        setStatus(message);
        emit failed(message);
    }
    // next in queue
    processNext();
}

void DownloadManager::resolveAndDownload(const QString &trackId, const QString &baseName, bool toMusic)
{
    m_baseName = baseName;
    m_toMusic = toMusic;
    m_candidates.clear();
    m_candIndex = -1;
    setBusy(true);
    setStatus(tr("Fetching stream…"));
    m_api->get(QStringLiteral("/tracks/%1").arg(trackId), QVariantMap(),
               QStringLiteral("dl-track"));
}

void DownloadManager::onApiSuccess(const QString &tag, const QVariantMap &data)
{
    if (tag == QLatin1String("dl-track")) {
        const QVariantList trans = data.value(QStringLiteral("media")).toMap()
                .value(QStringLiteral("transcodings")).toList();
        for (const QVariant &tv : trans) {
            const QVariantMap t = tv.toMap();
            const QString url = t.value(QStringLiteral("url")).toString();
            const QString preset = t.value(QStringLiteral("preset")).toString();
            const QString protocol = t.value(QStringLiteral("format")).toMap()
                    .value(QStringLiteral("protocol")).toString();
            if (url.isEmpty() || preset.contains(QLatin1String("ctr")) || preset.contains(QLatin1String("cbc")))
                continue;
            const bool progressive = protocol == QLatin1String("progressive")
                    || protocol == QLatin1String("http")
                    || url.contains(QLatin1String("/stream/progressive"));
            // include HLS (SoundCloud is HLS-only for many tracks)
            if (progressive && preset.contains(QLatin1String("mp3")))
                m_candidates.prepend(url);
            else if (progressive)
                m_candidates.append(url);
            else if (preset.contains(QLatin1String("abr")))
                m_candidates.append(url); // last
            else
                m_candidates.append(url);
        }
        if (m_candidates.isEmpty()) {
            finishJob(false, tr("No downloadable stream"));
            return;
        }
        m_candIndex = 0;
        setStatus(tr("Resolving…"));
        m_api->get(m_candidates.at(0), QVariantMap(), QStringLiteral("dl-media"));
        return;
    }

    if (tag == QLatin1String("dl-media")) {
        const QString realUrl = data.value(QStringLiteral("url")).toString();
        if (realUrl.isEmpty()) {
            if (m_candIndex + 1 < m_candidates.size()) {
                ++m_candIndex;
                m_api->get(m_candidates.at(m_candIndex), QVariantMap(), QStringLiteral("dl-media"));
                return;
            }
            finishJob(false, tr("Could not resolve download URL"));
            return;
        }
        startDownload(QUrl(realUrl), m_baseName, m_toMusic);
    }
}

void DownloadManager::onApiFailed(const QString &tag, int status, const QString &message)
{
    if (!tag.startsWith(QLatin1String("dl-")))
        return;
    Q_UNUSED(status)
    if (tag == QLatin1String("dl-media") && m_candIndex + 1 < m_candidates.size()) {
        ++m_candIndex;
        m_api->get(m_candidates.at(m_candIndex), QVariantMap(), QStringLiteral("dl-media"));
        return;
    }
    finishJob(false, message.isEmpty() ? tr("Download failed") : message);
}

void DownloadManager::startDownload(const QUrl &url, const QString &baseName, bool toMusic)
{
    setStatus(tr("Downloading…"));

    QString dir = toMusic ? musicDir()
                          : QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                            + QStringLiteral("/streams");
    QDir().mkpath(dir);

    const QString s = url.toString();
    // HLS playlist: download and concatenate AAC/TS segments into one file
    if (s.contains(QLatin1String(".m3u8")) || s.contains(QLatin1String("playlist"))) {
        downloadHls(url, dir + QLatin1Char('/') + baseName + QStringLiteral(".m4a"));
        return;
    }

    const QString ext = s.contains(QLatin1String(".m4a")) || s.contains(QLatin1String("aac"))
            ? QStringLiteral(".m4a") : QStringLiteral(".mp3");
    const QString path = dir + QLatin1Char('/') + baseName + ext;

    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    req.setRawHeader("Referer", "https://soundcloud.com/");
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(120000);
    loop.exec();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        reply->deleteLater();
        finishJob(false, tr("Download failed (%1)").arg(status));
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reply->deleteLater();
        finishJob(false, tr("Cannot write %1").arg(path));
        return;
    }
    f.write(reply->readAll());
    f.close();
    reply->deleteLater();

    setStatus(tr("Saved to %1").arg(path));
    emit finished(path);
    finishJob(true, QString());
}

void DownloadManager::downloadHls(const QUrl &playlistUrl, const QString &outPath)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(playlistUrl);
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(60000);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        finishJob(false, tr("HLS playlist failed"));
        return;
    }
    const QString playlist = QString::fromUtf8(reply->readAll());
    reply->deleteLater();

    QStringList segs;
    const QStringList lines = playlist.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString t = line.trimmed();
        if (t.isEmpty() || t.startsWith(QLatin1Char('#')))
            continue;
        segs.append(t.startsWith(QLatin1String("http")) ? t
                    : playlistUrl.resolved(QUrl(t)).toString());
    }
    if (segs.isEmpty()) {
        finishJob(false, tr("No HLS segments"));
        return;
    }

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        finishJob(false, tr("Cannot write %1").arg(outPath));
        return;
    }

    int i = 0;
    for (const QString &segUrl : segs) {
        setStatus(tr("Downloading… %1%").arg(100 * i / segs.size()));
        const QUrl seg(segUrl);
        QNetworkRequest sreq(seg);
        sreq.setRawHeader("User-Agent",
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        sreq.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        QNetworkReply *sr = nam.get(sreq);
        QEventLoop el;
        QTimer t2;
        t2.setSingleShot(true);
        connect(&t2, &QTimer::timeout, &el, &QEventLoop::quit);
        connect(sr, &QNetworkReply::finished, &el, &QEventLoop::quit);
        t2.start(30000);
        el.exec();
        if (sr->error() == QNetworkReply::NoError)
            out.write(sr->readAll());
        sr->deleteLater();
        ++i;
    }
    out.close();

    setStatus(tr("Saved to %1").arg(outPath));
    emit finished(outPath);
    finishJob(true, QString());
}
