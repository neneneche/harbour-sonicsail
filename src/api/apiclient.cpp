#include "apiclient.h"
#include "../auth/authmanager.h"
#include "../models/trackmodel.h"
#include "jsonparser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QTimer>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QScopedPointer>
#include <QDebug>

static const char kApiV1[] = "https://api.soundcloud.com";
static const char kApiV2[] = "https://api-v2.soundcloud.com";

static QSettings *scSettings()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + QStringLiteral("/harbour-soundsail.conf");
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    return new QSettings(path, QSettings::NativeFormat);
}

ApiClient::ApiClient(AuthManager *auth, QObject *parent)
    : QObject(parent)
    , m_auth(auth)
{
    connect(m_auth, &AuthManager::tokenReady, this, &ApiClient::onTokenReady);
    connect(m_auth, &AuthManager::tokenFailed, this, &ApiClient::onTokenFailed);

    QScopedPointer<QSettings> s(scSettings());
    m_publicClientId = s->value(QStringLiteral("public_client_id")).toString();
}

void ApiClient::setPublicClientId(const QString &id)
{
    m_publicClientId = id;
    emit publicClientIdChanged();
    {
        QScopedPointer<QSettings> s(scSettings());
        s->setValue(QStringLiteral("public_client_id"), id);
    }
    const QList<Pending> items = m_waitingClientId;
    m_waitingClientId.clear();
    for (const Pending &item : items)
        dispatch(item);
}

void ApiClient::invalidatePublicClientId()
{
    m_publicClientId.clear();
    emit publicClientIdChanged();
    QScopedPointer<QSettings> s(scSettings());
    s->remove(QStringLiteral("public_client_id"));
}

bool ApiClient::usePublicApi() const
{
    // Web oauth tokens and the scraped client_id only work with api-v2.
    // api.soundcloud.com (v1) returns 403 for anything except a paid developer app.
    return true;
}

QUrl ApiClient::buildUrl(const QString &path, const QVariantMap &query) const
{
    QUrl url;
    if (path.startsWith(QLatin1String("http"))) {
        // never touch api.soundcloud.com — rewrite to api-v2
        QString p = path;
        p.replace(QStringLiteral("https://api.soundcloud.com"),
                  QStringLiteral("https://api-v2.soundcloud.com"));
        p.replace(QStringLiteral("http://api.soundcloud.com"),
                  QStringLiteral("https://api-v2.soundcloud.com"));
        url = QUrl(p);
    } else {
        QString p = path;
        if (!p.startsWith(QLatin1Char('/')))
            p.prepend(QLatin1Char('/'));
        url = QUrl(QString::fromLatin1(kApiV2) + p);
    }

    QUrlQuery q = QUrlQuery(url.query());
    for (auto it = query.constBegin(); it != query.constEnd(); ++it)
        q.addQueryItem(it.key(), it.value().toString());

    if (!m_publicClientId.isEmpty() && !q.hasQueryItem(QStringLiteral("client_id")))
        q.addQueryItem(QStringLiteral("client_id"), m_publicClientId);
    url.setQuery(q);
    return url;
}

QNetworkRequest ApiClient::makeRequest(const QUrl &url, bool authorized) const
{
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    if (authorized && !m_token.isEmpty())
        req.setRawHeader("Authorization", ("OAuth " + m_token).toUtf8());
    return req;
}

void ApiClient::ensureThenDispatch(const Pending &p)
{
    // Guest/api-v2: just need client_id
    if (!m_publicClientId.isEmpty()) {
        dispatch(p);
        return;
    }

    m_waitingClientId.append(p);
}

void ApiClient::dispatch(const Pending &p)
{
    QUrl url = p.nextHref.isEmpty() ? buildUrl(p.path, p.query) : QUrl(p.nextHref);
    if (!p.nextHref.isEmpty()) {
        // rewrite any leftover v1 host in next_href
        QString s = url.toString();
        s.replace(QStringLiteral("https://api.soundcloud.com"),
                  QStringLiteral("https://api-v2.soundcloud.com"));
        url = QUrl(s);
        QUrlQuery q(url.query());
        for (auto it = p.query.constBegin(); it != p.query.constEnd(); ++it)
            q.addQueryItem(it.key(), it.value().toString());
        if (!m_publicClientId.isEmpty() && !q.hasQueryItem(QStringLiteral("client_id")))
            q.addQueryItem(QStringLiteral("client_id"), m_publicClientId);
        url.setQuery(q);
    }

    QNetworkReply *reply = nullptr;
    const bool authorized = true;
    if (p.method == QLatin1String("POST")) {
        QNetworkRequest req = makeRequest(url, authorized);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = m_nam.post(req, QJsonDocument::fromVariant(p.body).toJson(QJsonDocument::Compact));
    } else if (p.method == QLatin1String("PUT")) {
        QNetworkRequest req = makeRequest(url, authorized);
        // empty body: web-app shaped PUTs (e.g. likes) send no payload
        if (p.body.isEmpty())
            reply = m_nam.put(req, QByteArray());
        else {
            req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            reply = m_nam.put(req, QJsonDocument::fromVariant(p.body).toJson(QJsonDocument::Compact));
        }
    } else if (p.method == QLatin1String("DELETE")) {
        reply = m_nam.deleteResource(makeRequest(url, authorized));
    } else {
        reply = m_nam.get(makeRequest(url, authorized));
    }

    m_pending.insert(reply, p);

    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->start(15000);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, &ApiClient::onFinished);
}

QObject *ApiClient::createTrackModel()
{
    return new TrackModel(this, this);
}

QString ApiClient::userId() const
{
    return m_auth ? m_auth->userId() : QString();
}

void ApiClient::get(const QString &path, const QVariantMap &query, const QString &tag)
{
    Pending p;
    p.method = QStringLiteral("GET");
    p.path = path;
    p.query = query;
    p.tag = tag;
    ensureThenDispatch(p);
}

void ApiClient::getMore(const QString &nextHref, const QString &tag)
{
    Pending p;
    p.method = QStringLiteral("GET");
    p.nextHref = nextHref;
    p.tag = tag;
    ensureThenDispatch(p);
}

void ApiClient::post(const QString &path, const QVariantMap &body, const QString &tag)
{
    Pending p;
    p.method = QStringLiteral("POST");
    p.path = path;
    p.body = body;
    p.tag = tag;
    ensureThenDispatch(p);
}

void ApiClient::put(const QString &path, const QVariantMap &body, const QString &tag)
{
    Pending p;
    p.method = QStringLiteral("PUT");
    p.path = path;
    p.body = body;
    p.tag = tag;
    ensureThenDispatch(p);
}

void ApiClient::del(const QString &path, const QString &tag)
{
    Pending p;
    p.method = QStringLiteral("DELETE");
    p.path = path;
    p.tag = tag;
    ensureThenDispatch(p);
}

void ApiClient::postSilent(const QString &path)
{
    Pending p;
    p.method = QStringLiteral("POST");
    p.path = path;
    p.tag = QStringLiteral("silent");
    ensureThenDispatch(p);
}

void ApiClient::deleteSilent(const QString &path)
{
    Pending p;
    p.method = QStringLiteral("DELETE");
    p.path = path;
    p.tag = QStringLiteral("silent");
    ensureThenDispatch(p);
}

void ApiClient::onFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    Pending p = m_pending.take(reply);
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    if ((status == 401 || status == 403) && !p.retried) {
        p.retried = true;
        invalidatePublicClientId();
        m_waitingClientId.append(p);
        emit unauthorized();
        // trigger rediscovery via AuthManager's resolver chain — AppController owns it;
        // as a fallback just fail after retry
        QTimer::singleShot(0, this, [this]() {
            // if still no client_id, the queued items fail on next failed resolve
            if (!m_publicClientId.isEmpty()) {
                const QList<Pending> items = m_waitingClientId;
                m_waitingClientId.clear();
                for (const Pending &item : items)
                    dispatch(item);
            }
        });
        return;
    }

    if (reply->error() != QNetworkReply::NoError && status != 200 && status != 201) {
        if (p.tag == QLatin1String("silent"))
            return;
        // 404 on optional api-v2 endpoints — short message, no scary transfer text
        if (status == 404)
            emit failed(p.tag, status, tr("Not found"));
        else
            emit failed(p.tag, status, reply->errorString());
        return;
    }

    if (p.tag == QLatin1String("silent"))
        return;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError) {
        emit failed(p.tag, status, QStringLiteral("bad response"));
        return;
    }

    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        // Player and downloader need raw stream/transcoding fields — pass the object as-is.
        // Must run BEFORE the collection branch: /tracks/{id}/related has a "collection" key
        // but the player still needs the raw map (and its nested shape).
        if (p.tag.startsWith(QLatin1String("player"))
                || p.tag.startsWith(QLatin1String("dl"))) {
            emit success(p.tag, obj.toVariantMap());
            return;
        }
        if (obj.contains(QStringLiteral("collection"))) {
            const QVariantList items = JsonParser::parseCollection(obj);
            QString next = obj.value(QStringLiteral("next_href")).toString();
            emit listReceived(p.tag, items, next);
            return;
        }
        if (obj.value(QStringLiteral("kind")).toString() == QLatin1String("track"))
            emit success(p.tag, JsonParser::trackMap(obj));
        else if (obj.value(QStringLiteral("kind")).toString() == QLatin1String("user"))
            emit success(p.tag, JsonParser::userMap(obj));
        else
            emit success(p.tag, obj.toVariantMap());
        return;
    }

    if (doc.isArray()) {
        emit listReceived(p.tag, doc.array().toVariantList(), QString());
        return;
    }

    emit failed(p.tag, status, QStringLiteral("unexpected payload"));
}

void ApiClient::onTokenReady(const QString &token)
{
    m_token = token;
}

void ApiClient::onTokenFailed(const QString &message)
{
    Q_UNUSED(message)
    m_token.clear();
}
