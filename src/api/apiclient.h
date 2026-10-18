#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>

class AuthManager;

class ApiClient : public QObject
{
    Q_OBJECT
public:
    explicit ApiClient(AuthManager *auth, QObject *parent = nullptr);

    Q_INVOKABLE void get(const QString &path, const QVariantMap &query = QVariantMap(),
                         const QString &tag = QString());
    Q_INVOKABLE void getMore(const QString &nextHref, const QString &tag = QString());
    Q_INVOKABLE void post(const QString &path, const QVariantMap &body = QVariantMap(),
                          const QString &tag = QString());
    Q_INVOKABLE void put(const QString &path, const QVariantMap &body = QVariantMap(),
                         const QString &tag = QString());
    Q_INVOKABLE void del(const QString &path, const QString &tag = QString());
    Q_INVOKABLE QObject *createTrackModel();

    void setPublicClientId(const QString &id);
    bool usePublicApi() const;
    QString publicClientId() const { return m_publicClientId; }
    void invalidatePublicClientId();

    // fire-and-forget helpers used by player
    void postSilent(const QString &path);
    void deleteSilent(const QString &path);

signals:
    void success(const QString &tag, const QVariantMap &data);
    void failed(const QString &tag, int status, const QString &message);
    void listReceived(const QString &tag, const QVariantList &items, const QString &nextHref);
    void unauthorized();

private slots:
    void onFinished();
    void onTokenReady(const QString &token);
    void onTokenFailed(const QString &message);

private:
    struct Pending {
        QString method;
        QString path;
        QVariantMap query;
        QVariantMap body;
        QString tag;
        QString nextHref;
        bool retried = false;
    };

    QNetworkRequest makeRequest(const QUrl &url, bool authorized) const;
    void dispatch(const Pending &p);
    void ensureThenDispatch(const Pending &p);
    QUrl buildUrl(const QString &path, const QVariantMap &query) const;

    AuthManager *m_auth = nullptr;
    QNetworkAccessManager m_nam;
    QHash<QNetworkReply *, Pending> m_pending;
    QList<Pending> m_waitingClientId;
    QString m_token;
    QString m_publicClientId;
};
