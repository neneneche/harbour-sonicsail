#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

class Settings;
class TokenStore;

class AuthManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ isLoggedIn NOTIFY authStateChanged)
    Q_PROPERTY(bool hasCredentials READ hasCredentials NOTIFY authStateChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY authStateChanged)
    Q_PROPERTY(QString mode READ modeString NOTIFY authStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    explicit AuthManager(Settings *settings, QObject *parent = nullptr);

    bool isLoggedIn() const { return m_loggedIn; }
    bool hasCredentials() const;
    QString userName() const { return m_userName; }
    QString modeString() const;
    bool busy() const { return m_busy; }

    // Returns a usable access token (mints/refreshes as needed). Empty on failure.
    Q_INVOKABLE void ensureToken();
    Q_INVOKABLE void login();
    Q_INVOKABLE void loginWithPassword(const QString &email, const QString &password);
    Q_INVOKABLE void loginWithToken(const QString &token);
    Q_INVOKABLE void importBrowserCookies();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void cancelLogin();

    QString currentToken() const { return m_accessToken; }
    bool isUserMode() const { return m_mode == ModeUser; }

    void setBusy(bool busy);

signals:
    void authStateChanged();
    void busyChanged();
    void tokenReady(const QString &token);
    void tokenFailed(const QString &message);
    void loginFailed(const QString &message);
    void loginSucceeded();

private slots:
    void onTokenReply();
    void onPasswordReply();
    void onTokenVerifyReply();
    void onAuthorizeFinished();

private:
    enum Mode { ModeNone, ModeAnon, ModeUser };

    void mintClientCredentials();
    void refreshUserToken();
    void exchangeCode(const QString &code);
    void applyToken(const QJsonObject &json, const QString &mode);
    void startPkceLogin();
    static QString signPassword(const QString &user, const QString &password, const QString &clientId);
    QNetworkRequest tokenRequest() const;
    void postToken(const QUrlQuery &form);

    Settings *m_settings = nullptr;
    TokenStore *m_store = nullptr;
    QNetworkAccessManager m_nam;
    QNetworkReply *m_pending = nullptr;

    Mode m_mode = ModeNone;
    QString m_accessToken;
    QString m_refreshToken;
    qint64 m_expiresAt = 0;
    QString m_userName;
    bool m_loggedIn = false;
    bool m_busy = false;
    bool m_ensurePending = false;

    QString m_pkceVerifier;
    QString m_pkceState;
    QString m_redirectUri;
    QString m_pendingToken;
};
