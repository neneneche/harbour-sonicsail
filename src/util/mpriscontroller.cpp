#include "mpriscontroller.h"
#include "../player/playercontroller.h"
#include "../player/trackqueue.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>

static const char kService[] = "org.mpris.MediaPlayer2.harbour-soundsail";
static const char kPath[] = "/org/mpris/MediaPlayer2";

static QString trackObjectPath(const QString &seed)
{
    const QByteArray hash = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Md5)
            .toHex().left(16);
    return QStringLiteral("/org/mpris/MediaPlayer2/track/%1")
            .arg(QString::fromLatin1(hash));
}

// ---------- root adaptor ----------

MprisRootAdaptor::MprisRootAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

void MprisRootAdaptor::Raise()
{
}

void MprisRootAdaptor::Quit()
{
    QCoreApplication::quit();
}

// ---------- player adaptor ----------

MprisPlayerAdaptor::MprisPlayerAdaptor(PlayerController *player, QObject *parent)
    : QDBusAbstractAdaptor(parent)
    , m_player(player)
{
    setAutoRelaySignals(true);
    connect(m_player, &PlayerController::stateChanged,
            this, &MprisPlayerAdaptor::onPlayerChanged);
    connect(m_player, &PlayerController::trackChanged,
            this, &MprisPlayerAdaptor::onPlayerChanged);
    connect(m_player, &PlayerController::shuffleChanged,
            this, &MprisPlayerAdaptor::onPlayerChanged);
    connect(m_player, &PlayerController::repeatModeChanged,
            this, &MprisPlayerAdaptor::onPlayerChanged);
}

QString MprisPlayerAdaptor::playbackStatus() const
{
    if (m_player->isPlaying())
        return QStringLiteral("Playing");
    if (m_player->title().isEmpty())
        return QStringLiteral("Stopped");
    return QStringLiteral("Paused");
}

QString MprisPlayerAdaptor::loopStatus() const
{
    switch (m_player->repeatMode()) {
    case 1: return QStringLiteral("Playlist");
    case 2: return QStringLiteral("Track");
    default: return QStringLiteral("None");
    }
}

void MprisPlayerAdaptor::setLoopStatus(const QString &s)
{
    if (s == QLatin1String("Playlist"))
        m_player->setRepeatMode(1);
    else if (s == QLatin1String("Track"))
        m_player->setRepeatMode(2);
    else
        m_player->setRepeatMode(0);
}

bool MprisPlayerAdaptor::shuffle() const
{
    return m_player->shuffle();
}

void MprisPlayerAdaptor::setShuffle(bool on)
{
    m_player->setShuffle(on);
}

QVariantMap MprisPlayerAdaptor::metadata() const
{
    QVariantMap md;
    const QString title = m_player->title();
    const QString artist = m_player->artist();
    if (!title.isEmpty()) {
        md.insert(QStringLiteral("xesam:title"), title);
        md.insert(QStringLiteral("mpris:trackid"),
                  QVariant::fromValue(QDBusObjectPath(
                      trackObjectPath(m_player->permalinkUrl() + title))));
    }
    if (!artist.isEmpty()) {
        md.insert(QStringLiteral("xesam:artist"), QStringList{artist});
        md.insert(QStringLiteral("xesam:albumArtist"), QStringList{artist});
    }
    if (!m_player->artworkUrl().isEmpty())
        md.insert(QStringLiteral("mpris:artUrl"), m_player->artworkUrl());
    if (m_player->duration() > 0)
        md.insert(QStringLiteral("mpris:length"), qlonglong(m_player->duration()) * 1000);
    if (!m_player->permalinkUrl().isEmpty())
        md.insert(QStringLiteral("xesam:url"), m_player->permalinkUrl());
    return md;
}

qlonglong MprisPlayerAdaptor::position() const
{
    return qlonglong(m_player->position()) * 1000;
}

void MprisPlayerAdaptor::Next() { m_player->next(); }
void MprisPlayerAdaptor::Previous() { m_player->previous(); }
void MprisPlayerAdaptor::Pause() { m_player->pause(); }
void MprisPlayerAdaptor::PlayPause() { m_player->toggle(); }
void MprisPlayerAdaptor::Stop() { m_player->stop(); }
void MprisPlayerAdaptor::Play() { m_player->play(); }

void MprisPlayerAdaptor::Seek(qlonglong offset)
{
    const qint64 target = qint64(m_player->position()) + offset / 1000;
    m_player->seek(int(qMax<qint64>(0, target)));
    emit Seeked(qlonglong(m_player->position()) * 1000);
}

void MprisPlayerAdaptor::SetPosition(const QDBusObjectPath &trackId, qlonglong position)
{
    Q_UNUSED(trackId)
    m_player->seek(int(position / 1000));
    emit Seeked(position);
}

void MprisPlayerAdaptor::OpenUri(const QString &uri)
{
    Q_UNUSED(uri)
}

void MprisPlayerAdaptor::onPlayerChanged()
{
    const QString status = playbackStatus();
    const QString title = m_player->title();
    QStringList names;
    if (status != m_lastStatus) {
        m_lastStatus = status;
        names << QStringLiteral("PlaybackStatus");
    }
    if (title != m_lastTitle) {
        m_lastTitle = title;
        names << QStringLiteral("Metadata");
    }
    names << QStringLiteral("LoopStatus") << QStringLiteral("Shuffle");
    emitProperties(names);
}

void MprisPlayerAdaptor::emitProperties(const QStringList &names)
{
    QVariantMap changed;
    for (const QString &n : names) {
        if (n == QLatin1String("PlaybackStatus"))
            changed.insert(n, playbackStatus());
        else if (n == QLatin1String("Metadata"))
            changed.insert(n, metadata());
        else if (n == QLatin1String("LoopStatus"))
            changed.insert(n, loopStatus());
        else if (n == QLatin1String("Shuffle"))
            changed.insert(n, shuffle());
    }
    if (changed.isEmpty())
        return;
    QDBusMessage msg = QDBusMessage::createSignal(
        QLatin1String(kPath),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    msg << QStringLiteral("org.mpris.MediaPlayer2.Player") << changed << QStringList();
    QDBusConnection::sessionBus().send(msg);
}

// ---------- service ----------

MprisService::MprisService(PlayerController *player, TrackQueue *queue, QObject *parent)
    : QObject(parent)
{
    Q_UNUSED(queue)
    m_root = new MprisRootAdaptor(this);
    m_player = new MprisPlayerAdaptor(player, this);
}

bool MprisService::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "MPRIS: session bus not connected";
        return false;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        qWarning() << "MPRIS: registerService failed" << bus.lastError().message();
        return false;
    }
    if (!bus.registerObject(QLatin1String(kPath), this,
                            QDBusConnection::ExportAdaptors)) {
        qWarning() << "MPRIS: registerObject failed" << bus.lastError().message();
        return false;
    }
    qWarning() << "MPRIS: registered" << kService;
    return true;
}
