#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QString>
#include <QSet>

class ApiClient;

class TrackModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
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
        KindRole,
        PlaybackCountRole,
        LikesCountRole,
        UserFavoriteRole
    };

    explicit TrackModel(ApiClient *api, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool busy() const { return m_busy; }
    bool hasMore() const { return !m_nextHref.isEmpty(); }
    QString errorString() const { return m_error; }

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void loadCharts();
    Q_INVOKABLE void loadMixes();
    Q_INVOKABLE void loadUserPlaylists();
    Q_INVOKABLE void clearItems();
    Q_INVOKABLE void loadUserTracks(const QString &urn);
    Q_INVOKABLE void loadLikes();
    Q_INVOKABLE void loadHistory();
    Q_INVOKABLE void loadPlaylistTracks(const QString &urn);
    Q_INVOKABLE void loadRelated(const QString &urn);
    Q_INVOKABLE void loadFeed();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setItems(const QVariantList &items);
    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void busyChanged();
    void hasMoreChanged();
    void errorStringChanged();
    void countChanged();

private slots:
    void onList(const QString &tag, const QVariantList &items, const QString &nextHref);
    void onSuccess(const QString &tag, const QVariantMap &data);
    void onFailed(const QString &tag, int status, const QString &message);

private:
    void resetTo(const QVariantList &items, const QString &nextHref);
    void appendItems(const QVariantList &items, const QString &nextHref);
    void setBusy(bool busy);
    void setError(const QString &e);
    void reload();
    void collectPlaylists(const QVariantList &items);
    void finishPlaylists();

    ApiClient *m_api = nullptr;
    QVariantList m_items;
    QString m_nextHref;
    QString m_lastTag;
    QString m_lastKind;
    QString m_lastArg;
    QString m_lastQuery;
    bool m_busy = false;
    QString m_error;
    bool m_appendMode = false;
    bool m_loadAll = false;
    int m_autoPages = 0;
    QString m_pageTag;
    QString m_lastNextHref;
    QVariantList m_plCollected;
    int m_plPending = 0;
    QSet<QString> m_plSeen;
    QSet<QString> m_plPageBusy;
    QString m_tagPrefix;
    bool ownsTag(const QString &tag) const;
    QString tagFor(const QString &kind) const;
    void continuePagination(const QString &tag, const QString &nextHref);
    void beginLoadAll();
};
