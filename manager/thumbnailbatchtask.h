#ifndef THUMBNAILBATCHTASK_H
#define THUMBNAILBATCHTASK_H

#include <QRunnable>
#include <QVector>
#include <QAtomicInt>
#include <QImage>
#include <QPointer>

class PerThreadMuPDFRenderer;
class ThumbnailCache;
class ThumbnailManagerV2;

enum class RenderPriority {
    IMMEDIATE,
    HIGH,
    MEDIUM,
    LOW
};

class ThumbnailBatchTask : public QRunnable
{
public:
    using FinishCallback = std::function<void()>;

    ThumbnailBatchTask(const QString& docPath,
                       ThumbnailCache* cache,
                       ThumbnailManagerV2* manager,
                       const QVector<int>& pageIndices,
                       RenderPriority priority,
                       int thumbnailWidth,
                       int rotation,
                       double devicePixelRatio,
                       FinishCallback cb);

    ~ThumbnailBatchTask();

    void run() override;
    void abort();
    bool isAborted() const;

private:
    int getTimeBudget() const;
    int getBatchLimit() const;

    std::unique_ptr<PerThreadMuPDFRenderer> m_renderer;
    ThumbnailCache* m_cache;
    ThumbnailManagerV2* m_manager;
    QVector<int> m_pageIndices;
    RenderPriority m_priority;
    int m_thumbnailWidth;
    int m_rotation;
    double m_devicePixelRatio;
    QAtomicInt m_aborted;

    FinishCallback m_finishCallback;
};

#endif // THUMBNAILBATCHTASK_H
