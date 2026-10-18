#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QVariantList>
#include <QSet>

class ApiClient;

class DownloadManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
public:
    explicit DownloadManager(ApiClient *api, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString statusMessage() const { return m_status; }

    // Resolve stream and save to Music/Soundsail (or cache if saveToMusic is false)
    Q_INVOKABLE void downloadTrack(const QVariantMap &track, bool saveToMusic);
    // Queue a track if not already saved/downloading (used for auto-download)
    Q_INVOKABLE void autoDownload(const QVariantMap &track, bool saveToMusic);

    QString musicDir() const;

signals:
    void busyChanged();
    void statusMessageChanged();
    void finished(const QString &path);
    void failed(const QString &message);

private slots:
    void onApiSuccess(const QString &tag, const QVariantMap &data);
    void onApiFailed(const QString &tag, int status, const QString &message);

private:
    struct Job {
        QString id;
        QString baseName;
        bool toMusic = true;
    };

    void setStatus(const QString &s);
    void setBusy(bool b);
    void startDownload(const QUrl &url, const QString &baseName, bool toMusic);
    void downloadHls(const QUrl &playlistUrl, const QString &outPath);
    void resolveAndDownload(const QString &trackId, const QString &baseName, bool toMusic);
    void enqueue(const Job &job);
    void processNext();
    void finishJob(bool ok, const QString &message);

    ApiClient *m_api = nullptr;
    bool m_busy = false;
    QString m_status;
    QString m_baseName;
    bool m_toMusic = true;
    QStringList m_candidates;
    int m_candIndex = -1;
    QList<Job> m_queue;
    QSet<QString> m_doneIds;
    QSet<QString> m_activeIds;
    QString m_currentId;
};
