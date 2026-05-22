#ifndef PAGECACHEMANAGER_H
#define PAGECACHEMANAGER_H

#include <QImage>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QHash>

struct PageCacheKey {
    int pageIndex;
    double zoom;
    int rotation;
    double dpr;   // devicePixelRatio：同一逻辑缩放下 Retina(2.0) 与普通屏(1.0) 的位图不可混用

    PageCacheKey(int page = -1, double z = 1.0, int rot = 0, double devicePixelRatio = 1.0)
        : pageIndex(page), zoom(z), rotation(rot), dpr(devicePixelRatio) {}

    bool operator==(const PageCacheKey& other) const {
        return pageIndex == other.pageIndex &&
               qAbs(zoom - other.zoom) < 0.001 &&
               rotation == other.rotation &&
               qAbs(dpr - other.dpr) < 0.001;
    }

    bool operator<(const PageCacheKey& other) const {
        if (pageIndex != other.pageIndex)
            return pageIndex < other.pageIndex;
        if (qAbs(zoom - other.zoom) >= 0.001)
            return zoom < other.zoom;
        if (rotation != other.rotation)
            return rotation < other.rotation;
        if (qAbs(dpr - other.dpr) >= 0.001)
            return dpr < other.dpr;
        return false;
    }

    uint hash() const {
        return qHash(pageIndex) ^ qHash(qRound(zoom * 1000))
               ^ qHash(rotation) ^ qHash(qRound(dpr * 1000));
    }

    QString toString() const {
        return QString("Page:%1,Zoom:%2,Rot:%3,DPR:%4")
        .arg(pageIndex).arg(zoom, 0, 'f', 2).arg(rotation).arg(dpr, 0, 'f', 2);
    }
};

inline uint qHash(const PageCacheKey& key, uint seed = 0) {
    return key.hash() ^ seed;
}

class PageCacheManager
{
public:
    enum class CacheStrategy {
        LRU,
        MRU,
        NearCurrent
    };

    explicit PageCacheManager(int maxSize = 10,
                              CacheStrategy strategy = CacheStrategy::NearCurrent);

    bool addPage(int pageIndex, double zoom, int rotation, double dpr, const QImage& image);

    QImage getPage(int pageIndex, double zoom, int rotation, double dpr);

    bool contains(int pageIndex, double zoom, int rotation, double dpr) const;

    void removePage(int pageIndex, double zoom, int rotation, double dpr);

    void clear();

    void clearByZoomRotation(double zoom = -1, int rotation = -1);

    void setMaxSize(int maxSize);

    int maxSize() const { return m_maxSize; }

    void setStrategy(CacheStrategy strategy);

    CacheStrategy strategy() const { return m_strategy; }

    int cacheSize() const;

    QList<PageCacheKey> cachedKeys() const;

    void setCurrentPage(int pageIndex, double zoom, int rotation);

    qint64 memoryUsage() const;

    void markVisiblePages(const QSet<int>& visiblePages);

    QString getStatistics() const;

private:
    void evict();

    PageCacheKey selectKeyToEvict();

    void updateAccessTime(const PageCacheKey& key);

private:
    mutable QMutex m_mutex;

    int m_maxSize;
    CacheStrategy m_strategy;

    QMap<PageCacheKey, QImage> m_cache;
    QMap<PageCacheKey, qint64> m_accessTime;
    QSet<int> m_visiblePages;

    PageCacheKey m_currentKey;
    qint64 m_timeCounter;

    qint64 m_hitCount;
    qint64 m_missCount;
};

#endif // PAGECACHEMANAGER_H
