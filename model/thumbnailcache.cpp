#include "thumbnailcache.h"
#include <QReadLocker>
#include <QWriteLocker>
#include <QCoreApplication>

ThumbnailCache::ThumbnailCache()
{
}

ThumbnailCache::~ThumbnailCache()
{
}

QImage ThumbnailCache::get(int pageIndex) const
{
    QReadLocker locker(&m_lock);
    return m_cache.value(pageIndex, QImage());
}

void ThumbnailCache::set(int pageIndex, const QImage& thumbnail)
{
    if (thumbnail.isNull()) {
        return;
    }

    QWriteLocker locker(&m_lock);
    m_cache[pageIndex] = thumbnail;
}

bool ThumbnailCache::has(int pageIndex) const
{
    QReadLocker locker(&m_lock);
    return m_cache.contains(pageIndex);
}

void ThumbnailCache::clear()
{
    QWriteLocker locker(&m_lock);
    m_cache.clear();
}

QString ThumbnailCache::getStatistics() const
{
    int cachedCount = count();

    qint64 memoryKB = cachedCount * 50;

    return QCoreApplication::translate("ThumbnailCache", "Thumbnail Cache: %1 pages (%.2 MB)")
        .arg(cachedCount)
        .arg(memoryKB / 1024.0, 0, 'f', 2);
}

int ThumbnailCache::count() const
{
    QReadLocker locker(&m_lock);
    return m_cache.size();
}
