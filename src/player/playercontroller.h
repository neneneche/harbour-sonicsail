#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QAbstractListModel>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ApiClient;
class TrackQueue;

class PlayerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool playing READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString artworkUrl READ artworkUrl NOTIFY trackChanged)
    Q_PROPERTY(QString permalinkUrl READ permalinkUrl NOTIFY trackChanged)
    Q_PROPERTY(QVariantMap currentTrack READ currentTrack NOTIFY trackChanged)
    Q_PROPERTY(QVariantList waveformSamples READ waveformSamples NOTIFY waveformChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool liked READ liked NOTIFY likedChanged)
public:
    explicit PlayerController(ApiClient *api, TrackQueue *queue, QObject *parent = nullptr);

    bool isPlaying() const;
    int position() const { return m_position; }
    int duration() const { return m_duration; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString artworkUrl() const { return m_artworkUrl; }
    QString permalinkUrl() const { return m_permalinkUrl; }
    bool busy() const { return m_busy; }
    QString statusMessage() const { return m_statusMessage; }
    bool shuffle() const { return m_shuffle; }
    void setShuffle(bool on);
    // 0 = off, 1 = repeat all, 2 = repeat one
    int repeatMode() const { return m_repeatMode; }
    void setRepeatMode(int mode);
    bool liked() const;
    Q_INVOKABLE void toggleCurrentLike();
    QVariantMap currentTrack() const;

    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void playTrack(const QVariantMap &track);
    Q_INVOKABLE void playQueue(const QVariantList &tracks, int startIndex);
    Q_INVOKABLE void playFromModel(QObject *model, int index);
    Q_INVOKABLE void startRadio(const QVariantMap &track);
    Q_INVOKABLE void shuffleQueue();
    Q_INVOKABLE void toggleLike(const QVariantMap &track);
    Q_INVOKABLE void likeTrack(const QString &urn, bool like);
    QVariantList waveformSamples() const { return m_waveformSamples; }
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(int positionMs);

signals:
    void stateChanged();
    void positionChanged();
    void durationChanged();
    void trackChanged();
    void busyChanged();
    void statusMessageChanged();
    void shuffleChanged();
    void repeatModeChanged();
    void likedChanged();
    void waveformChanged();
    void errorOccurred(const QString &message);

private slots:
    void onPlayerStateChanged(QMediaPlayer::State state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error);
    void onStreamsReceived(const QString &tag, const QVariantMap &data);
    void onRelatedReceived(const QString &tag, const QVariantMap &data);
    void onApiFailed(const QString &tag, int status, const QString &message);
    void onWaveformReply();
    void onTick();

private:
    void loadCurrent();
    void resolveStreams(const QString &urn);
    void tryPlay(const QUrl &url);
    void playResolved(const QUrl &url);
    void setBusy(bool busy);
    void setStatus(const QString &msg);
    void updateMetadata();
    void loadWaveform(const QVariantMap &track);
    void reportPlayed();
    void handlePlaybackFailure(const QString &message);

    ApiClient *m_api = nullptr;
    TrackQueue *m_queue = nullptr;
    QMediaPlayer *m_player = nullptr;
    QTimer m_tick;

    QString m_title;
    QString m_artist;
    QString m_artworkUrl;
    QString m_permalinkUrl;
    QString m_currentUrn;
    QStringList m_candidates;
    QString m_pendingLikeUrn;
    bool m_pendingLikeWanted = false;
    QStringList m_transcodingUrls;
    QString m_pendingTranscoding;
    QVariantList m_waveformSamples;
    QNetworkAccessManager m_waveNam;
    QNetworkReply *m_waveReply = nullptr;
    int m_candidateIndex = -1;
    int m_position = 0;
    int m_duration = 0;
    bool m_busy = false;
    QString m_statusMessage;
    bool m_reported = false;
    bool m_pendingPlay = false;
    int m_loadGen = 0;
    bool m_shuffle = false;
    int m_repeatMode = 0;
    int m_failStreak = 0;
};
