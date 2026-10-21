#include "clientidresolver.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QScopedPointer>
#include <QDebug>
#include <algorithm>

static QSettings *scSettings()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + QStringLiteral("/harbour-soundsail.conf");
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    return new QSettings(path, QSettings::NativeFormat);
}

ClientIdResolver::ClientIdResolver(QObject *parent)
    : QObject(parent)
{
    QScopedPointer<QSettings> s(scSettings());
    m_clientId = s->value(QStringLiteral("public_client_id")).toString();
}

void ClientIdResolver::reset()
{
    m_clientId.clear();
    m_started = false;
    QScopedPointer<QSettings> s(scSettings());
    s->remove(QStringLiteral("public_client_id"));
    emit readyChanged();
}

void ClientIdResolver::resolve()
{
    if (!m_clientId.isEmpty()) {
        emit resolved(m_clientId);
        return;
    }
    if (m_started)
        return;
    m_started = true;
    m_assetQueue.clear();

    QNetworkRequest req(QUrl(QStringLiteral("https://soundcloud.com/")));
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, &ClientIdResolver::onHtml);
}

void ClientIdResolver::onHtml()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    const QByteArray body = reply->readAll();
    tryExtractFrom(body);
    if (!m_clientId.isEmpty())
        return;

    // Collect script src URLs (yt-dlp style)
    static const QRegularExpression scriptRe(
        QStringLiteral("<script[^>]+src=\"([^\"]+)\""));
    const QString html = QString::fromUtf8(body);
    QRegularExpressionMatchIterator it = scriptRe.globalMatch(html);
    QStringList urls;
    while (it.hasNext())
        urls.append(it.next().captured(1));

    // Reverse order — client_id is typically in later bundles
    std::reverse(urls.begin(), urls.end());
    for (const QString &u : urls) {
        if (!m_assetQueue.contains(u))
            m_assetQueue.append(u);
    }

    if (m_assetQueue.isEmpty()) {
        emit failed(QStringLiteral("no scripts on soundcloud.com"));
        m_started = false;
        return;
    }

    const int limit = qMin(12, int(m_assetQueue.size()));
    for (int i = 0; i < limit; ++i) {
        QNetworkRequest req(QUrl(m_assetQueue.at(i)));
        req.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                         "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        QNetworkReply *r = m_nam.get(req);
        connect(r, &QNetworkReply::finished, this, &ClientIdResolver::onAsset);
    }
}

void ClientIdResolver::onAsset()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
        return;
    tryExtractFrom(reply->readAll());
}

void ClientIdResolver::tryExtractFrom(const QByteArray &body)
{
    if (!m_clientId.isEmpty())
        return;
    const QString text = QString::fromUtf8(body);

    // Exact pattern used by yt-dlp: 32 alphanumeric chars
    static const QRegularExpression re(
        QStringLiteral("client_id\\s*:\\s*\"([0-9a-zA-Z]{32})\""));
    QRegularExpressionMatch m = re.match(text);
    if (m.hasMatch()) {
        finish(m.captured(1));
        return;
    }

    // Fallback: client_id=... in URL/query strings inside JS
    static const QRegularExpression re2(
        QStringLiteral("client_id=([0-9a-zA-Z]{32})"));
    m = re2.match(text);
    if (m.hasMatch())
        finish(m.captured(1));
}

void ClientIdResolver::finish(const QString &id)
{
    m_clientId = id;
    m_started = false;
    QScopedPointer<QSettings> s(scSettings());
    s->setValue(QStringLiteral("public_client_id"), id);
    emit readyChanged();
    emit resolved(id);
    qWarning() << "ClientIdResolver: found client_id length" << id.length();
}
