#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>
#include "../api/apitypes.h"

class TrackQueue : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QVariantMap currentTrack READ currentTrack NOTIFY currentTrackChanged)
public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        UsernameRole,
        ArtworkUrlRole,
        DurationMsRole,
        DurationTextRole,
        UrnRole,
        PermalinkUrlRole,
        PlayableRole,
        AccessRole,
        IsCurrentRole
    };

    explicit TrackQueue(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const { return m_current; }
    void setCurrentIndex(int idx);
    QVariantMap currentTrack() const;

    Q_INVOKABLE void setFromList(const QVariantList &tracks, int startIndex = 0);
    Q_INVOKABLE void append(const QVariantMap &track);
    Q_INVOKABLE void insertNext(const QVariantMap &track);
    Q_INVOKABLE void removeAt(int idx);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QVariantMap get(int idx) const;
    Q_INVOKABLE void setFavorite(const QString &urn, bool favorite);

    const QVector<ScTrack> &tracks() const { return m_tracks; }
    ScTrack trackAt(int idx) const;
    bool hasNext() const { return m_current + 1 < m_tracks.size(); }
    bool hasPrevious() const { return m_current > 0; }

signals:
    void countChanged();
    void currentIndexChanged();
    void currentTrackChanged();

private:
    QVector<ScTrack> m_tracks;
    int m_current = -1;
};
