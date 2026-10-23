#include "settings.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QScopedPointer>

#ifndef SOUNDCLOUD_CLIENT_ID
#define SOUNDCLOUD_CLIENT_ID ""
#endif
#ifndef SOUNDCLOUD_CLIENT_SECRET
#define SOUNDCLOUD_CLIENT_SECRET ""
#endif

static QSettings *openSettings()
{
    // Sailjail sandbox: default QSettings(org, app) path is inaccessible.
    // Use AppConfigLocation as documented by sailjail-permissions.
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + QStringLiteral("/harbour-soundsail.conf");
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    return new QSettings(path, QSettings::NativeFormat);
}

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    m_quality = confValue(QStringLiteral("quality"));
    if (m_quality.isEmpty())
        m_quality = QStringLiteral("hls_aac_160");
    m_lastVolume = confValue(QStringLiteral("lastVolume"));
    if (m_lastVolume.isEmpty())
        m_lastVolume = QStringLiteral("1.0");
    if (confValue(QStringLiteral("saveToMusic")).isEmpty())
        setConfValue(QStringLiteral("saveToMusic"), QStringLiteral("true"));
}

QString Settings::confValue(const QString &key) const
{
    QScopedPointer<QSettings> s(openSettings());
    return s->value(key).toString();
}

void Settings::setConfValue(const QString &key, const QString &value)
{
    QScopedPointer<QSettings> s(openSettings());
    s->setValue(key, value);
}

bool Settings::hasClientCredentials() const
{
    return !clientId().isEmpty();
}

QString Settings::clientId() const
{
    if (!m_runtimeClientId.isEmpty())
        return m_runtimeClientId;
    const QString stored = confValue(QStringLiteral("client_id"));
    if (!stored.isEmpty())
        return stored;
    const QString compile = QString::fromUtf8(SOUNDCLOUD_CLIENT_ID);
    if (!compile.isEmpty())
        return compile;
    // public web player client_id discovered at runtime
    return confValue(QStringLiteral("public_client_id"));
}

QString Settings::clientSecret() const
{
    if (!m_runtimeClientSecret.isEmpty())
        return m_runtimeClientSecret;
    const QString stored = confValue(QStringLiteral("client_secret"));
    if (!stored.isEmpty())
        return stored;
    return QString::fromUtf8(SOUNDCLOUD_CLIENT_SECRET);
}

void Settings::setRuntimeCredentials(const QString &clientId, const QString &clientSecret)
{
    m_runtimeClientId = clientId.trimmed();
    m_runtimeClientSecret = clientSecret.trimmed();
    setConfValue(QStringLiteral("client_id"), m_runtimeClientId);
    setConfValue(QStringLiteral("client_secret"), m_runtimeClientSecret);
    emit credentialsChanged();
}

void Settings::clearCredentials()
{
    m_runtimeClientId.clear();
    m_runtimeClientSecret.clear();
    setConfValue(QStringLiteral("client_id"), QString());
    setConfValue(QStringLiteral("client_secret"), QString());
    emit credentialsChanged();
}

QString Settings::quality() const { return m_quality; }

void Settings::setQuality(const QString &q)
{
    if (m_quality == q)
        return;
    m_quality = q;
    setConfValue(QStringLiteral("quality"), q);
    emit qualityChanged();
}

QString Settings::lastVolume() const { return m_lastVolume; }

void Settings::setLastVolume(const QString &v)
{
    if (m_lastVolume == v)
        return;
    m_lastVolume = v;
    setConfValue(QStringLiteral("lastVolume"), v);
    emit lastVolumeChanged();
}

bool Settings::saveToMusic() const
{
    return confValue(QStringLiteral("saveToMusic")) != QLatin1String("false");
}

void Settings::setSaveToMusic(bool on)
{
    setConfValue(QStringLiteral("saveToMusic"), on ? QStringLiteral("true") : QStringLiteral("false"));
    emit saveToMusicChanged();
}

bool Settings::autoDownloadPlayed() const
{
    return confValue(QStringLiteral("autoDownloadPlayed")) == QLatin1String("true");
}

void Settings::setAutoDownloadPlayed(bool on)
{
    setConfValue(QStringLiteral("autoDownloadPlayed"), on ? QStringLiteral("true") : QStringLiteral("false"));
    emit autoDownloadPlayedChanged();
}

bool Settings::showWaveform() const
{
    return confValue(QStringLiteral("showWaveform")) == QLatin1String("true");
}

void Settings::setShowWaveform(bool on)
{
    setConfValue(QStringLiteral("showWaveform"), on ? QStringLiteral("true") : QStringLiteral("false"));
    emit showWaveformChanged();
}

bool Settings::showComments() const
{
    return confValue(QStringLiteral("showComments")) != QLatin1String("false");
}

void Settings::setShowComments(bool on)
{
    setConfValue(QStringLiteral("showComments"), on ? QStringLiteral("true") : QStringLiteral("false"));
    emit showCommentsChanged();
}

QString Settings::userId() const
{
    return confValue(QStringLiteral("userId"));
}

void Settings::setUserId(const QString &id)
{
    setConfValue(QStringLiteral("userId"), id);
}

void Settings::completeLoginWithCode(const QString &code)
{
    emit loginCodeProvided(code);
}

void Settings::notify(const QString &message)
{
    emit notifyRequested(message);
}
