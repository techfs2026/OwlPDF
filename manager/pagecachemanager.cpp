#include "pagecachemanager.h"
#include <QMutexLocker>
#include <QDebug>

PageCacheManager::PageCacheManager(int maxSize, CacheStrategy strategy)
    : m_maxSize(maxSize)
    , m_strategy(strategy)
    , m_currentKey(-1, 1.0, 0)
    , m_timeCounter(0)
    , m_hitCount(0)
    , m_missCount(0)
{
}

bool PageCacheManager::addPage(int pageIndex, double zoom, int rotation, double dpr, const QImage& image)
{
    if (image.isNull()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);

    PageCacheKey key(pageIndex, zoom, rotation, dpr);

    if (m_cache.contains(key)) {
        m_cache[key] = image;
        updateAccessTime(key);
        return true;
    }

    if (m_cache.size() >= m_maxSize) {
        evict();
    }

    m_cache.insert(key, image);
    updateAccessTime(key);

    return true;
}

QImage PageCacheManager::getPage(int pageIndex, double zoom, int rotation, double dpr)
{
    QMutexLocker locker(&m_mutex);

    PageCacheKey key(pageIndex, zoom, rotation, dpr);

    if (!m_cache.contains(key)) {
        m_missCount++;
        return QImage();
    }

    updateAccessTime(key);
    m_hitCount++;

    return m_cache.value(key);
}

bool PageCacheManager::contains(int pageIndex, double zoom, int rotation, double dpr) const
{
    QMutexLocker locker(&m_mutex);
    PageCacheKey key(pageIndex, zoom, rotation, dpr);
    return m_cache.contains(key);
}

void PageCacheManager::removePage(int pageIndex, double zoom, int rotation, double dpr)
{
    QMutexLocker locker(&m_mutex);
    PageCacheKey key(pageIndex, zoom, rotation, dpr);
    m_cache.remove(key);
    m_accessTime.remove(key);
}

void PageCacheManager::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
    m_accessTime.clear();
    m_visiblePages.clear();
    m_timeCounter = 0;
    m_hitCount = 0;
    m_missCount = 0;
}

void PageCacheManager::clearByZoomRotation(double zoom, int rotation)
{
    QMutexLocker locker(&m_mutex);

    QList<PageCacheKey> keysToRemove;

    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        const PageCacheKey& key = it.key();

        bool shouldRemove = false;

        if (zoom >= 0 && rotation >= 0) {
            shouldRemove = (qAbs(key.zoom - zoom) < 0.001 && key.rotation == rotation);
        } else if (zoom >= 0) {
            shouldRemove = (qAbs(key.zoom - zoom) < 0.001);
        } else if (rotation >= 0) {
            shouldRemove = (key.rotation == rotation);
        } else {
            shouldRemove = true;
        }

        if (shouldRemove) {
            keysToRemove.append(key);
        }
    }

    for (const PageCacheKey& key : keysToRemove) {
        m_cache.remove(key);
        m_accessTime.remove(key);
    }
}

void PageCacheManager::setMaxSize(int maxSize)
{
    QMutexLocker locker(&m_mutex);

    if (maxSize < 1) {
        maxSize = 1;
    }

    m_maxSize = maxSize;

    while (m_cache.size() > m_maxSize) {
        evict();
    }
}

void PageCacheManager::setStrategy(CacheStrategy strategy)
{
    QMutexLocker locker(&m_mutex);
    m_strategy = strategy;
}

int PageCacheManager::cacheSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.size();
}

QList<PageCacheKey> PageCacheManager::cachedKeys() const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.keys();
}

void PageCacheManager::setCurrentPage(int pageIndex, double zoom, int rotation)
{
    QMutexLocker locker(&m_mutex);
    m_currentKey = PageCacheKey(pageIndex, zoom, rotation);
}

qint64 PageCacheManager::memoryUsage() const
{
    QMutexLocker locker(&m_mutex);

    qint64 totalBytes = 0;

    for (const QImage& image : m_cache) {
        totalBytes += image.sizeInBytes();
    }

    return totalBytes;
}

void PageCacheManager::markVisiblePages(const QSet<int>& visiblePages)
{
    QMutexLocker locker(&m_mutex);
    m_visiblePages = visiblePages;
}

QString PageCacheManager::getStatistics() const
{
    QMutexLocker locker(&m_mutex);

    qint64 totalAccess = m_hitCount + m_missCount;
    double hitRate = (totalAccess > 0) ? (m_hitCount * 100.0 / totalAccess) : 0;

    return QString("Cache: %1/%2 pages, Memory: %3 MB, Hit Rate: %4%, Hits: %5, Misses: %6")
        .arg(m_cache.size())
        .arg(m_maxSize)
        .arg(memoryUsage() / 1024.0 / 1024.0, 0, 'f', 2)
        .arg(hitRate, 0, 'f', 1)
        .arg(m_hitCount)
        .arg(m_missCount);
}

void PageCacheManager::evict()
{
    if (m_cache.isEmpty()) {
        return;
    }

    PageCacheKey keyToRemove = selectKeyToEvict();

    m_cache.remove(keyToRemove);
    m_accessTime.remove(keyToRemove);
}

PageCacheKey PageCacheManager::selectKeyToEvict()
{
    QList<PageCacheKey> cachedKeys = m_cache.keys();

    if (cachedKeys.isEmpty()) {
        return PageCacheKey();
    }

    PageCacheKey selectedKey = cachedKeys.first();

    switch (m_strategy) {
    case CacheStrategy::LRU: {
        qint64 oldestTime = m_accessTime.value(selectedKey, 0);

        for (const PageCacheKey& key : cachedKeys) {
            qint64 accessTime = m_accessTime.value(key, 0);
            if (accessTime < oldestTime) {
                oldestTime = accessTime;
                selectedKey = key;
            }
        }
        break;
    }

    case CacheStrategy::MRU: {
        qint64 newestTime = m_accessTime.value(selectedKey, 0);

        for (const PageCacheKey& key : cachedKeys) {
            qint64 accessTime = m_accessTime.value(key, 0);
            if (accessTime > newestTime) {
                newestTime = accessTime;
                selectedKey = key;
            }
        }
        break;
    }

    case CacheStrategy::NearCurrent: {
        QList<PageCacheKey> evictCandidates;

        for (const PageCacheKey& key : cachedKeys) {
            bool isVisibleAndMatch = m_visiblePages.contains(key.pageIndex) &&
                                     qAbs(key.zoom - m_currentKey.zoom) < 0.001 &&
                                     key.rotation == m_currentKey.rotation;

            if (!isVisibleAndMatch) {
                evictCandidates.append(key);
            }
        }

        if (!evictCandidates.isEmpty()) {
            cachedKeys = evictCandidates;
            selectedKey = cachedKeys.first();
        }

        double maxScore = 0;

        for (const PageCacheKey& key : cachedKeys) {
            int pageDistance = qAbs(key.pageIndex - m_currentKey.pageIndex);
            double zoomDistance = qAbs(key.zoom - m_currentKey.zoom);
            int rotationDistance = (key.rotation != m_currentKey.rotation) ? 1 : 0;

            double score = pageDistance * 100.0 + zoomDistance * 50.0 + rotationDistance * 25.0;

            if (score > maxScore) {
                maxScore = score;
                selectedKey = key;
            }
        }
        break;
    }
    }

    return selectedKey;
}

void PageCacheManager::updateAccessTime(const PageCacheKey& key)
{
    m_accessTime[key] = ++m_timeCounter;
}
