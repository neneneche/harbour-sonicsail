#include "authmanager.h"
#include "tokenstore.h"
#include "pkce.h"
#include "../util/settings.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QTimer>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <pwd.h>
#include <unistd.h>

static const char kAuthBase[] = "https://secure.soundcloud.com";
static const char kApiBase[] = "https://api.soundcloud.com";

AuthManager::AuthManager(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_store(new TokenStore(this))
    , m_redirectUri(QStringLiteral("http://localhost:8080/oauth/callback"))
{
    // Restore cached user token if present
    const TokenStore::Token user = m_store->load(QStringLiteral("user"));
    if (user.isValid(QDateTime::currentMSecsSinceEpoch())) {
        m_mode = ModeUser;
        m_accessToken = user.accessToken;
        m_refreshToken = user.refreshToken;
        m_expiresAt = user.expiresAt;
        m_loggedIn = true;
        // re-apply to ApiClient after connections are wired (AppController)
        QTimer::singleShot(0, this, [this]() {
            emit tokenReady(m_accessToken);
            emit authStateChanged();
        });
    } else if (!user.accessToken.isEmpty()) {
        // token exists but marked expired — keep it for web oauth tokens (often long-lived)
        m_mode = ModeUser;
        m_accessToken = user.accessToken;
        m_expiresAt = QDateTime::currentMSecsSinceEpoch() + qint64(24) * 3600 * 1000;
        m_loggedIn = true;
        TokenStore::Token t = user;
        t.expiresAt = m_expiresAt;
        m_store->save(t);
        QTimer::singleShot(0, this, [this]() {
            emit tokenReady(m_accessToken);
            emit authStateChanged();
        });
    } else {
        const TokenStore::Token anon = m_store->load(QStringLiteral("anon"));
        if (anon.isValid(QDateTime::currentMSecsSinceEpoch())) {
            m_mode = ModeAnon;
            m_accessToken = anon.accessToken;
            m_expiresAt = anon.expiresAt;
            QTimer::singleShot(0, this, [this]() { emit tokenReady(m_accessToken); });
        }
    }
    // persist across restarts so like/unlike keeps working before the next /me
    m_userId = m_settings->userId();
}

bool AuthManager::hasCredentials() const
{
    return m_settings && m_settings->hasClientCredentials();
}

QString AuthManager::modeString() const
{
    switch (m_mode) {
    case ModeUser: return QStringLiteral("user");
    case ModeAnon: return QStringLiteral("anon");
    default: return QStringLiteral("none");
    }
}

void AuthManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void AuthManager::ensureToken()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_accessToken.isEmpty() && m_expiresAt - 60000 > now) {
        emit tokenReady(m_accessToken);
        return;
    }
    if (m_ensurePending)
        return;
    m_ensurePending = true;

    // Try refresh if we have a user refresh token
    if (m_mode == ModeUser && !m_refreshToken.isEmpty()) {
        refreshUserToken();
        return;
    }
    mintClientCredentials();
}

QNetworkRequest AuthManager::tokenRequest() const
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kAuthBase) + QStringLiteral("/oauth/token")));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("Accept", "application/json");
    return req;
}

void AuthManager::postToken(const QUrlQuery &form)
{
    setBusy(true);
    if (m_pending) {
        m_pending->disconnect(this);
        m_pending->abort();
        m_pending = nullptr;
    }
    m_pending = m_nam.post(tokenRequest(), form.toString(QUrl::FullyEncoded).toUtf8());
    connect(m_pending, &QNetworkReply::finished, this, &AuthManager::onTokenReply);
}

void AuthManager::mintClientCredentials()
{
    if (!hasCredentials()) {
        m_ensurePending = false;
        emit tokenFailed(QStringLiteral("credentials not configured"));
        return;
    }

    QNetworkRequest req = tokenRequest();
    const QByteArray basic = (m_settings->clientId() + QLatin1Char(':') + m_settings->clientSecret())
            .toUtf8().toBase64();
    req.setRawHeader("Authorization", "Basic " + basic);

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("client_credentials"));

    setBusy(true);
    if (m_pending) {
        m_pending->disconnect(this);
        m_pending->abort();
    }
    m_pending = m_nam.post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(m_pending, &QNetworkReply::finished, this, &AuthManager::onTokenReply);
}

void AuthManager::refreshUserToken()
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    form.addQueryItem(QStringLiteral("client_id"), m_settings->clientId());
    if (!m_settings->clientSecret().isEmpty())
        form.addQueryItem(QStringLiteral("client_secret"), m_settings->clientSecret());
    postToken(form);
}

void AuthManager::exchangeCode(const QString &code)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    form.addQueryItem(QStringLiteral("client_id"), m_settings->clientId());
    form.addQueryItem(QStringLiteral("code_verifier"), m_pkceVerifier);
    if (!m_settings->clientSecret().isEmpty()) {
        QNetworkRequest req = tokenRequest();
        const QByteArray basic = (m_settings->clientId() + QLatin1Char(':') + m_settings->clientSecret())
                .toUtf8().toBase64();
        req.setRawHeader("Authorization", "Basic " + basic);
        setBusy(true);
        m_pending = m_nam.post(req, form.toString(QUrl::FullyEncoded).toUtf8());
        connect(m_pending, &QNetworkReply::finished, this, &AuthManager::onTokenReply);
    } else {
        postToken(form);
    }
}

void AuthManager::applyToken(const QJsonObject &json, const QString &mode)
{
    m_accessToken = json.value(QStringLiteral("access_token")).toString();
    m_refreshToken = json.value(QStringLiteral("refresh_token")).toString();
    const int expiresIn = json.value(QStringLiteral("expires_in")).toInt(3600);
    m_expiresAt = QDateTime::currentMSecsSinceEpoch() + qint64(expiresIn) * 1000;
    m_mode = (mode == QLatin1String("user")) ? ModeUser : ModeAnon;

    TokenStore::Token t;
    t.accessToken = m_accessToken;
    t.refreshToken = m_refreshToken;
    t.expiresAt = m_expiresAt;
    t.tokenType = json.value(QStringLiteral("token_type")).toString();
    t.scope = json.value(QStringLiteral("scope")).toString();
    t.mode = mode;
    // Persist BEFORE further use (refresh_token is single-use)
    m_store->save(t);

    if (mode == QLatin1String("user")) {
        m_loggedIn = true;
        emit loginSucceeded();
    }
    emit authStateChanged();
}

void AuthManager::onTokenReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply == m_pending)
        m_pending = nullptr;
    setBusy(false);
    m_ensurePending = false;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    if (reply->error() != QNetworkReply::NoError && status != 200) {
        const QString msg = QStringLiteral("token request failed (%1)").arg(status);
        qWarning() << "AuthManager:" << msg;
        // fallback: try anonymous mint on refresh failure
        if (m_mode == ModeUser && status >= 400) {
            m_loggedIn = false;
            m_mode = ModeNone;
            m_accessToken.clear();
            m_refreshToken.clear();
            m_store->clear(QStringLiteral("user"));
            emit authStateChanged();
            mintClientCredentials();
            return;
        }
        emit tokenFailed(msg);
        emit loginFailed(msg);
        return;
    }

    const QJsonObject json = QJsonDocument::fromJson(body).object();
    const QString access = json.value(QStringLiteral("access_token")).toString();
    if (access.isEmpty()) {
        emit tokenFailed(QStringLiteral("no access_token in response"));
        return;
    }

    // Decide mode: if we were exchanging a user code/refresh, stay user; else anon
    const QString mode = (m_loggedIn || m_mode == ModeUser || !m_pkceVerifier.isEmpty())
            ? QStringLiteral("user") : QStringLiteral("anon");
    if (!m_pkceVerifier.isEmpty())
        m_pkceVerifier.clear();

    applyToken(json, mode);
    emit tokenReady(m_accessToken);
}

void AuthManager::startPkceLogin()
{
    if (!hasCredentials()) {
        emit loginFailed(QStringLiteral("credentials not configured"));
        return;
    }

    m_pkceVerifier = Pkce::generateVerifier();
    const QString challenge = Pkce::challengeS256(m_pkceVerifier);
    m_pkceState = Pkce::generateVerifier().left(16);

    QUrl url(QString::fromLatin1(kAuthBase) + QStringLiteral("/authorize"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_id"), m_settings->clientId());
    q.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("code_challenge"), challenge);
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    q.addQueryItem(QStringLiteral("state"), m_pkceState);
    q.addQueryItem(QStringLiteral("scope"),
                   QStringLiteral("non-expiring user-info user-library user-followings"));
    url.setQuery(q);

    // Fallback path: open system browser. A later Amber.Web.Authorization
    // integration can replace this without changing the token exchange.
    QDesktopServices::openUrl(url);
    setBusy(true);
    emit authStateChanged();
}

void AuthManager::login()
{
    startPkceLogin();
}

// Rolling 24-bit hash used by SoundCloud's web-auth sign-in (from yt-dlp reference)
QString AuthManager::signPassword(const QString &user, const QString &password, const QString &clientId)
{
    Q_UNUSED(password)
    const int a = 33, i = 1, s = 440123, w = 117, u = 1800000, l = 1042, b = 37, k = 37;
    const QString n = QStringLiteral("0763ed7314c69015fd4a0dc16bbf4b90");
    const QString y = QStringLiteral("8");
    const QString ua = QStringLiteral(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/84.0.4147.105 Safari/537.36");
    const QString d = QStringLiteral("%1-%2-%3-%4-%5-%6-%7-%8")
            .arg(a).arg(i).arg(s).arg(w).arg(u).arg(l).arg(b).arg(k);
    const QString h = n + y + d + ua + user + clientId + d + n;

    quint32 m = 8011470;
    for (int f = 0; f < h.size(); ++f) {
        m = (m >> 1) + ((1 & m) << 23);
        m += h.at(f).unicode();
        m &= 16777215;
    }
    return QStringLiteral("%1:%2:%3:%4").arg(y).arg(d)
            .arg(m, 0, 16).arg(5);
}

void AuthManager::loginWithPassword(const QString &email, const QString &password)
{
    if (!hasCredentials() && m_settings->clientId().isEmpty()) {
        // still try with whatever client_id ApiClient discovered via settings
    }
    const QString clientId = m_settings->clientId();
    if (clientId.isEmpty()) {
        emit loginFailed(tr("No client id — try again after charts load"));
        return;
    }
    if (email.isEmpty() || password.isEmpty()) {
        emit loginFailed(tr("Enter email and password"));
        return;
    }

    setBusy(true);

    // device_id: 4 blocks of 6 digits
    QString deviceId;
    for (int b = 0; b < 4; ++b) {
        if (b) deviceId += QLatin1Char('-');
        for (int j = 0; j < 6; ++j)
            deviceId += QString::number(QDateTime::currentMSecsSinceEpoch() % 10);
    }

    QJsonObject creds;
    creds.insert(QStringLiteral("identifier"), email);
    creds.insert(QStringLiteral("password"), password);

    QJsonObject payload;
    payload.insert(QStringLiteral("client_id"), clientId);
    payload.insert(QStringLiteral("recaptcha_pubkey"), QStringLiteral("null"));
    payload.insert(QStringLiteral("recaptcha_response"), QStringLiteral("null"));
    payload.insert(QStringLiteral("credentials"), creds);
    payload.insert(QStringLiteral("signature"), signPassword(email, password, clientId));
    payload.insert(QStringLiteral("device_id"), deviceId);
    payload.insert(QStringLiteral("user_agent"),
                   QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                  "AppleWebKit/537.36 (KHTML, like Gecko) "
                                  "Chrome/84.0.4147.105 Safari/537.36"));

    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api-auth.soundcloud.com/web-auth/sign-in/password?client_id=%1").arg(clientId)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");

    if (m_pending) {
        m_pending->disconnect(this);
        m_pending->abort();
    }
    m_pending = m_nam.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(m_pending, &QNetworkReply::finished, this, &AuthManager::onPasswordReply);
}

void AuthManager::onPasswordReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply == m_pending)
        m_pending = nullptr;
    setBusy(false);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    if (status < 200 || status >= 300) {
        const QString msg = tr("Sign-in failed (%1)").arg(status);
        qWarning() << "AuthManager password login:" << status << body.left(200);
        emit loginFailed(msg);
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonObject session = root.value(QStringLiteral("session")).toObject();
    const QString token = session.value(QStringLiteral("access_token")).toString();
    if (token.isEmpty()) {
        emit loginFailed(tr("No access token in response"));
        return;
    }

    m_accessToken = token;
    m_mode = ModeUser;
    m_loggedIn = true;
    const int expiresIn = session.value(QStringLiteral("expires_in")).toInt(3600);
    m_expiresAt = QDateTime::currentMSecsSinceEpoch() + qint64(expiresIn) * 1000;

    TokenStore::Token t;
    t.accessToken = m_accessToken;
    t.refreshToken = session.value(QStringLiteral("refresh_token")).toString();
    t.expiresAt = m_expiresAt;
    t.tokenType = QStringLiteral("OAuth");
    t.mode = QStringLiteral("user");
    m_store->save(t);

    emit authStateChanged();
    emit loginSucceeded();
    emit tokenReady(m_accessToken);
}

static QString realHome()
{
    // Sailjail sandbox redirects HomeLocation; use the actual account home
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0])
        return QString::fromLocal8Bit(pw->pw_dir);
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

static QStringList cookieDbCandidates()
{
    QStringList roots;
    const QString home = realHome();
    roots << home + QStringLiteral("/.org.sailfishos/browser/.mozilla")
          << home + QStringLiteral("/.cache/org.sailfishos/browser/.mozilla")
          << home + QStringLiteral("/.local/share/org.sailfishos/browser/.mozilla")
          << home + QStringLiteral("/.mozilla/mozembed")
          << home + QStringLiteral("/.mozilla")
          << QStringLiteral("/home/defaultuser/.cache/org.sailfishos/browser/.mozilla")
          << QStringLiteral("/home/defaultuser/.local/share/org.sailfishos/browser/.mozilla")
          << QStringLiteral("/home/defaultuser/.org.sailfishos/browser/.mozilla")
          << QStringLiteral("/home/nemo/.cache/org.sailfishos/browser/.mozilla")
          << QStringLiteral("/home/nemo/.local/share/org.sailfishos/browser/.mozilla")
          << QStringLiteral("/home/nemo/.org.sailfishos/browser/.mozilla")
          << QStringLiteral("/home/nemo/.mozilla/mozembed");
    QStringList dbs;
    for (const QString &root : roots) {
        if (!QDir(root).exists())
            continue;
        QDirIterator it(root, QStringList() << QStringLiteral("cookies.sqlite"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            dbs.append(it.next());
        const QString direct = root + QStringLiteral("/cookies.sqlite");
        if (QFile::exists(direct) && !dbs.contains(direct))
            dbs.append(direct);
    }
    return dbs;
}

static QString queryOauthToken(const QString &dbPath)
{
    static bool s_sqlWarned = false;
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (!s_sqlWarned) {
            s_sqlWarned = true;
            qWarning() << "AuthManager: QSQLITE driver unavailable — cannot read cookie databases";
        }
        return QString();
    }

    // SQLite may be in WAL mode — copy main db + wal/shm so we see recent cookies
    const QString tmpBase = QDir::tempPath() + QStringLiteral("/soundsail-cookies.sqlite");
    QFile::remove(tmpBase);
    QFile::remove(tmpBase + QStringLiteral("-wal"));
    QFile::remove(tmpBase + QStringLiteral("-shm"));
    if (!QFile::copy(dbPath, tmpBase))
        return QString();
    QFile::setPermissions(tmpBase, QFile::ReadOwner | QFile::WriteOwner);
    QFile::copy(dbPath + QStringLiteral("-wal"), tmpBase + QStringLiteral("-wal"));
    QFile::copy(dbPath + QStringLiteral("-shm"), tmpBase + QStringLiteral("-shm"));

    QString token;
    {
        const QString connName = QStringLiteral("soundsail-cookies");
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(tmpBase);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral(
                    "SELECT value FROM moz_cookies WHERE name = 'oauth_token' "
                    "AND (host LIKE '%soundcloud.com') "
                    "ORDER BY LENGTH(host) DESC LIMIT 1")) && q.next()) {
                token = q.value(0).toString();
            }
            if (token.isEmpty()) {
                // fallback: any cookie value that looks like a long token on soundcloud
                if (q.exec(QStringLiteral(
                        "SELECT name, value FROM moz_cookies "
                        "WHERE host LIKE '%soundcloud.com'")) ) {
                    while (q.next()) {
                        const QString name = q.value(0).toString();
                        const QString value = q.value(1).toString();
                        if (name.contains(QLatin1String("oauth"), Qt::CaseInsensitive)
                                && value.length() > 20) {
                            token = value;
                            break;
                        }
                    }
                }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }
    QFile::remove(tmpBase);
    QFile::remove(tmpBase + QStringLiteral("-wal"));
    QFile::remove(tmpBase + QStringLiteral("-shm"));
    return token;
}

void AuthManager::importBrowserCookies()
{
    const QStringList dbs = cookieDbCandidates();
    if (dbs.isEmpty()) {
        emit loginFailed(tr("No browser cookie database found. Open soundcloud.com in the Sailfish browser and sign in first."));
        return;
    }

    QString token;
    for (const QString &dbPath : dbs) {
        token = queryOauthToken(dbPath);
        if (!token.isEmpty())
            break;
    }

    if (token.isEmpty()) {
        emit loginFailed(tr("No SoundCloud login cookie found in the browser (%1 databases checked). Sign in at soundcloud.com in the Sailfish browser, then try again.")
                         .arg(dbs.size()));
        return;
    }
    loginWithToken(token);
}

static QStringList webProfileRoots()
{
    // Sailfish.WebView initializes its Gecko profile from a QStandardPaths
    // location (CacheLocation in current/older plugin versions; be tolerant
    // and also cover DataLocation variants).
    QStringList roots;
    const QStringList locations = QStringList()
            << QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            << QStandardPaths::writableLocation(QStandardPaths::DataLocation)
            << QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    for (const QString &loc : locations) {
        if (!loc.isEmpty() && !roots.contains(loc))
            roots.append(loc);
    }
    return roots;
}

static QStringList ownWebProfileCookieDbs()
{
    // The cookie store lives inside this app's own sandbox.
    static bool s_loggedRoots = false;
    const QStringList roots = webProfileRoots();

    QStringList dbs;
    for (const QString &root : roots) {
        if (!QDir(root).exists())
            continue;
        QDirIterator it(root, QStringList() << QStringLiteral("cookies.sqlite"),
                        QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext())
            dbs.append(it.next());
    }
    if (!s_loggedRoots) {
        s_loggedRoots = true;
        qInfo() << "AuthManager: web profile roots:" << roots << "cookie dbs:" << dbs;
    }
    return dbs;
}

void AuthManager::importOwnWebProfileCookies()
{
    if (m_busy || m_loggedIn)
        return;

    static bool s_loggedEmpty = false;
    const QStringList dbs = ownWebProfileCookieDbs();
    for (const QString &dbPath : dbs) {
        const QString token = queryOauthToken(dbPath);
        if (!token.isEmpty() && token != m_lastRejectedHarvest) {
            qInfo() << "AuthManager: harvested oauth_token from" << dbPath
                    << "len" << token.size();
            loginWithToken(token);
            return;
        }
    }
    if (!s_loggedEmpty && dbs.isEmpty()) {
        s_loggedEmpty = true;
        qWarning() << "AuthManager: no cookies.sqlite found under web profile roots yet";
    }
    // Silent: the WebView page keeps polling until sign-in sets the cookie.
}

int AuthManager::clearWebProfileSession()
{
    int removed = 0;
    for (const QString &root : webProfileRoots()) {
        if (!QDir(root).exists())
            continue;
        // cookie databases (hidden dirs like .mozilla must be visited)
        QDirIterator it(root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString name = it.fileName();
            if (name == QLatin1String("cookies.sqlite")
                    || name == QLatin1String("cookies.sqlite-wal")
                    || name == QLatin1String("cookies.sqlite-shm")
                    || name == QLatin1String("cookies.sqlite-journal")
                    || name == QLatin1String("webappsstore.sqlite")
                    || name == QLatin1String("webappsstore.sqlite-wal")
                    || name == QLatin1String("webappsstore.sqlite-shm")) {
                if (QFile::remove(path))
                    ++removed;
            }
        }
        // site storage for soundcloud (localStorage etc. — modern Gecko keeps
        // session material here too, not only in cookies.sqlite)
        QDirIterator dirIt(root, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                           QDirIterator::Subdirectories);
        QStringList soundcloudDirs;
        while (dirIt.hasNext()) {
            dirIt.next();
            if (dirIt.fileName().contains(QLatin1String("soundcloud"), Qt::CaseInsensitive))
                soundcloudDirs.append(dirIt.filePath());
        }
        for (const QString &dir : soundcloudDirs) {
            if (QDir(dir).removeRecursively())
                ++removed;
        }
    }
    qInfo() << "AuthManager: cleared web session files/dirs:" << removed;
    return removed;
}

void AuthManager::loginWithToken(const QString &token)
{
    const QString trimmed = token.trimmed();
    if (trimmed.isEmpty()) {
        emit loginFailed(tr("Enter an access token"));
        return;
    }

    // Store immediately — /connect/session often returns 400 and must not block login
    m_accessToken = trimmed;
    m_pendingToken.clear();
    m_mode = ModeUser;
    m_loggedIn = true;
    m_expiresAt = QDateTime::currentMSecsSinceEpoch() + qint64(24) * 3600 * 1000;

    TokenStore::Token t;
    t.accessToken = m_accessToken;
    t.expiresAt = m_expiresAt;
    t.tokenType = QStringLiteral("OAuth");
    t.mode = QStringLiteral("user");
    m_store->save(t);

    emit tokenReady(m_accessToken);
    emit authStateChanged();

    // Validate with GET /me (the only check that matters)
    setBusy(true);
    const QString clientId = m_settings->clientId();
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api-v2.soundcloud.com/me?client_id=%1").arg(clientId)));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", ("OAuth " + m_accessToken).toUtf8());
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    if (m_pending) {
        m_pending->disconnect(this);
        m_pending->abort();
    }
    m_pending = m_nam.get(req);
    connect(m_pending, &QNetworkReply::finished, this, &AuthManager::onTokenVerifyReply);
}

void AuthManager::onTokenVerifyReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply == m_pending)
        m_pending = nullptr;
    setBusy(false);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    if (status == 200) {
        // optional: pull username from /me
        const QJsonObject me = QJsonDocument::fromJson(body).object();
        m_userName = me.value(QStringLiteral("username")).toString();
        m_userId = QString::number(qint64(me.value(QStringLiteral("id")).toDouble()));
        m_settings->setUserId(m_userId);
        // commit state before navigation reacts to loginSucceeded
        emit authStateChanged();
        emit loginSucceeded();
        return;
    }

    // Token rejected — roll back
    if (status >= 400 && status < 500)
        m_lastRejectedHarvest = m_accessToken;
    m_loggedIn = false;
    m_mode = ModeNone;
    m_accessToken.clear();
    m_userName.clear();
    m_userId.clear();
    m_settings->setUserId(QString());
    m_store->clear(QStringLiteral("user"));
    emit authStateChanged();
    emit loginFailed(tr("Token rejected (%1) — copy oauth_token from soundcloud.com cookies").arg(status));
}

void AuthManager::cancelLogin()
{
    m_pkceVerifier.clear();
    m_pkceState.clear();
    setBusy(false);
    emit authStateChanged();
}

void AuthManager::logout()
{
    m_loggedIn = false;
    m_mode = ModeNone;
    m_accessToken.clear();
    m_refreshToken.clear();
    m_expiresAt = 0;
    m_userName.clear();
    m_userId.clear();
    m_settings->setUserId(QString());
    m_lastRejectedHarvest.clear();
    m_sessionCheckPending = false;
    m_store->clear(QStringLiteral("user"));
    emit authStateChanged();
    // re-mint anonymous token for continued browsing
    mintClientCredentials();
}

void AuthManager::refreshSession()
{
    if (!m_loggedIn || m_accessToken.isEmpty())
        return;

    emit sessionRefreshing();

    const QString clientId = m_settings->clientId();
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api-v2.soundcloud.com/me?client_id=%1").arg(clientId)));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", ("OAuth " + m_accessToken).toUtf8());
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    if (m_pending) {
        m_pending->disconnect(this);
        m_pending->abort();
    }
    m_pending = m_nam.get(req);
    connect(m_pending, &QNetworkReply::finished, this, &AuthManager::onSessionCheckReply);
}

void AuthManager::onSessionCheckReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();
    if (reply == m_pending)
        m_pending = nullptr;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    if (status == 200) {
        const QJsonObject me = QJsonDocument::fromJson(body).object();
        m_userName = me.value(QStringLiteral("username")).toString();
        m_userId = QString::number(qint64(me.value(QStringLiteral("id")).toDouble()));
        m_settings->setUserId(m_userId);
        qInfo() << "AuthManager: session refreshed for" << m_userName;
        emit sessionRefreshed(m_userName);
        emit authStateChanged();
        return;
    }

    if (status == 401) {
        // token genuinely dead — try one silent re-harvest from the web profile.
        // (403 is bot protection, NOT a bad token — never destroy the session on it)
        qInfo() << "AuthManager: stored session rejected (" << status
                << "), trying web profile re-harvest";
        m_sessionCheckPending = true;
        m_lastRejectedHarvest = m_accessToken;
        m_loggedIn = false;
        m_mode = ModeNone;
        m_accessToken.clear();
        m_userName.clear();
        m_userId.clear();
    m_settings->setUserId(QString());
        m_store->clear(QStringLiteral("user"));
        emit authStateChanged();
        importOwnWebProfileCookies();
        QTimer::singleShot(3000, this, [this]() {
            if (!m_sessionCheckPending)
                return;
            m_sessionCheckPending = false;
            if (m_loggedIn) {
                emit sessionRefreshed(m_userName);
            } else {
                qInfo() << "AuthManager: session refresh failed";
                emit sessionExpired();
                emit authStateChanged();
                mintClientCredentials();
            }
        });
        return;
    }

    // transient error (network, 5xx, 403 bot-protection) — keep the session
    qWarning() << "AuthManager: session check failed transiently" << status;
}

void AuthManager::onAuthorizeFinished()
{
    // placeholder for Amber.Web.Authorization hook
}
