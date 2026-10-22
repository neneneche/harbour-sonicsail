#pragma once

#include <QObject>
#include <QString>

class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasClientCredentials READ hasClientCredentials NOTIFY credentialsChanged)
    Q_PROPERTY(QString clientId READ clientId NOTIFY credentialsChanged)
    Q_PROPERTY(QString quality READ quality WRITE setQuality NOTIFY qualityChanged)
    Q_PROPERTY(QString lastVolume READ lastVolume WRITE setLastVolume NOTIFY lastVolumeChanged)
    Q_PROPERTY(bool saveToMusic READ saveToMusic WRITE setSaveToMusic NOTIFY saveToMusicChanged)
    Q_PROPERTY(bool autoDownloadPlayed READ autoDownloadPlayed WRITE setAutoDownloadPlayed NOTIFY autoDownloadPlayedChanged)
    Q_PROPERTY(bool showWaveform READ showWaveform WRITE setShowWaveform NOTIFY showWaveformChanged)
public:
    explicit Settings(QObject *parent = nullptr);

    bool hasClientCredentials() const;
    QString clientId() const;
    QString clientSecret() const;

    QString quality() const;
    void setQuality(const QString &q);

    QString lastVolume() const;
    void setLastVolume(const QString &v);

    bool saveToMusic() const;
    void setSaveToMusic(bool on);

    bool autoDownloadPlayed() const;
    void setAutoDownloadPlayed(bool on);
    bool showWaveform() const;
    void setShowWaveform(bool on);

    Q_INVOKABLE void setRuntimeCredentials(const QString &clientId, const QString &clientSecret);
    Q_INVOKABLE void clearCredentials();
    Q_INVOKABLE void completeLoginWithCode(const QString &code);
    Q_INVOKABLE void notify(const QString &message);

    // used by pages; safe to call before UI is ready
    Q_INVOKABLE void showToast(const QString &message) { emit notifyRequested(message); }

signals:
    void credentialsChanged();
    void qualityChanged();
    void lastVolumeChanged();
    void saveToMusicChanged();
    void autoDownloadPlayedChanged();
    void showWaveformChanged();
    void loginCodeProvided(const QString &code);
    void notifyRequested(const QString &message);

private:
    QString confValue(const QString &key) const;
    void setConfValue(const QString &key, const QString &value);

    QString m_runtimeClientId;
    QString m_runtimeClientSecret;
    QString m_quality;
    QString m_lastVolume;
};
