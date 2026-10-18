#include "coverimageprovider.h"

#include <QStandardPaths>
#include <QDir>

CoverImageProvider::CoverImageProvider(ApiClient *api)
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_api(api)
{
}

QString CoverImageProvider::cachePath(const QString &url) const
{
    Q_UNUSED(url)
    return QString();
}

QPixmap CoverImageProvider::loadFromDisk(const QString &path) const
{
    Q_UNUSED(path)
    return QPixmap();
}

void CoverImageProvider::saveToDisk(const QString &path, const QPixmap &pix) const
{
    Q_UNUSED(path)
    Q_UNUSED(pix)
}

QPixmap CoverImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(id)
    // Never block the GUI thread with network I/O here.
    // QML loads artwork via the https URL directly (asynchronous: true).
    const QSize sz = requestedSize.isValid() ? requestedSize : QSize(100, 100);
    QPixmap pix(sz);
    pix.fill(Qt::transparent);
    if (size)
        *size = sz;
    return pix;
}
