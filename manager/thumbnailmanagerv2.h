#ifndef THUMBNAILMANAGER_V2_H
#define THUMBNAILMANAGER_V2_H

#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <QTimer>
#include <memory>

#include "thumbnailbatchtask.h"
#include "thumbnailloadstrategy.h"

class PerThreadMuPDFRenderer;
class ThumbnailCache;

class ThumbnailManagerV2 : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailManagerV2(PerThreadMuPDFRenderer* renderer, QObject* parent = nullptr);
    ~ThumbnailManagerV2();

    void setThumbnailWidth(int width);
    void setRotation(int rotation);

    QImage getThumbnail(int pageIndex) const;
    bool hasThumbnail(int pageIndex) const;

    void startLoading(const QSet<int>& initialVisible);

    void syncLoadPages(const QVector<int>& pages);

    void handleSlowScroll(const QSet<int>& visiblePages);

    void cancelAllTasks();

    void clear();
    QString getStatistics() const;
    int cachedCount() const;

    bool shouldRespondToScroll() const;

    ThumbnailLoadStrategy* thumbnailLoadStrategy() const {return m_strategy.get();}

signals:
    void thumbnailLoaded(int pageIndex, const QImage& thumbnail);
    void loadProgress(int loaded, int total);
    void batchCompleted(int batchIndex, int totalBatches);
    void allCompleted();

    void loadingStarted(int totalPages, const QString& strategy);
    void loadingStatusChanged(const QString& status);

private slots:
    void processNextBatch();

private:
    void detectDevicePixelRatio();

    int getRenderWidth() const;

    void renderPagesSync(const QVector<int>& pages);

    void renderPagesAsync(const QVector<int>& pages, RenderPriority priority);

    void setupBackgroundBatches();

    void trackTask(ThumbnailBatchTask* task);

private:
    PerThreadMuPDFRenderer* m_renderer;
    std::unique_ptr<ThumbnailCache> m_cache;
    std::unique_ptr<QThreadPool> m_threadPool;
    std::unique_ptr<ThumbnailLoadStrategy> m_strategy;

    int m_thumbnailWidth;
    int m_rotation;
    double m_devicePixelRatio;

    QVector<QVector<int>> m_backgroundBatches;
    int m_nextBatchIndex = 0;
    int m_runningTasks = 0;

    QMutex m_taskMutex;

    bool m_isLoadingInProgress;
};

#endif // THUMBNAILMANAGER_V2_H
