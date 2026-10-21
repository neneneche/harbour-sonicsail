#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

// Discovers a public SoundCloud web-player client_id so the app can use
// api-v2.soundcloud.com without a paid developer app registration.
class ClientIdResolver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)
public:
    explicit ClientIdResolver(QObject *parent = nullptr);

    bool isReady() const { return !m_clientId.isEmpty(); }
    QString clientId() const { return m_clientId; }

    Q_INVOKABLE void resolve();
    Q_INVOKABLE void reset();

signals:
    void readyChanged();
    void resolved(const QString &clientId);
    void failed(const QString &message);

private slots:
    void onHtml();
    void onAsset();

private:
    void tryExtractFrom(const QByteArray &body);
    void finish(const QString &id);

    QNetworkAccessManager m_nam;
    QString m_clientId;
    QStringList m_assetQueue;
    bool m_started = false;
};
