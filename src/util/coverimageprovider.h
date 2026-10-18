#pragma once

#include <QQuickImageProvider>
#include <QNetworkAccessManager>
#include <QPixmap>
#include <QHash>
#include <QMutex>

class ApiClient;

class CoverImageProvider : public QQuickImageProvider
{
public:
    explicit CoverImageProvider(ApiClient *api);

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QString cachePath(const QString &url) const;
    QPixmap loadFromDisk(const QString &path) const;
    void saveToDisk(const QString &path, const QPixmap &pix) const;

    ApiClient *m_api = nullptr;
    QNetworkAccessManager m_nam;
    mutable QMutex m_mutex;
};
