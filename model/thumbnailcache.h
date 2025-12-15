#ifndef THUMBNAILCACHE_H
#define THUMBNAILCACHE_H

#include <QImage>
#include <QHash>
#include <QReadWriteLock>

class ThumbnailCache
{
public:
    ThumbnailCache();
    ~ThumbnailCache();

    QImage get(int pageIndex) const;
    void set(int pageIndex, const QImage& thumbnail);
    bool has(int pageIndex) const;

    void clear();
    QString getStatistics() const;
    int count() const;

private:
    QHash<int, QImage> m_cache;
    mutable QReadWriteLock m_lock;
};

#endif // THUMBNAILCACHE_H
