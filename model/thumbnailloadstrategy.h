#ifndef THUMBNAILLOADSTRATEGY_H
#define THUMBNAILLOADSTRATEGY_H

#include <QObject>
#include <QVector>
#include <QSet>

enum class LoadStrategyType {
    SMALL_DOC,
    MEDIUM_DOC,
    LARGE_DOC
};

class ThumbnailLoadStrategy : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailLoadStrategy(int pageCount, QObject* parent = nullptr)
        : QObject(parent), m_pageCount(pageCount) {}

    virtual ~ThumbnailLoadStrategy() = default;

    virtual LoadStrategyType type() const = 0;

    virtual QVector<int> getInitialLoadPages(const QSet<int>& visiblePages) const = 0;

    virtual QVector<QVector<int>> getBackgroundBatches() const = 0;

    virtual QVector<int> handleVisibleChange(const QSet<int>& visiblePages) const = 0;

protected:
    int m_pageCount;
};

class SmallDocStrategy : public ThumbnailLoadStrategy
{
    Q_OBJECT

public:
    explicit SmallDocStrategy(int pageCount, QObject* parent = nullptr);

    LoadStrategyType type() const override { return LoadStrategyType::SMALL_DOC; }
    QVector<int> getInitialLoadPages(const QSet<int>& visiblePages) const override;
    QVector<QVector<int>> getBackgroundBatches() const override;
    QVector<int> handleVisibleChange(const QSet<int>& visiblePages) const override;
};

class MediumDocStrategy : public ThumbnailLoadStrategy
{
    Q_OBJECT

public:
    explicit MediumDocStrategy(int pageCount, QObject* parent = nullptr);

    LoadStrategyType type() const override { return LoadStrategyType::MEDIUM_DOC; }
    QVector<int> getInitialLoadPages(const QSet<int>& visiblePages) const override;
    QVector<QVector<int>> getBackgroundBatches() const override;
    QVector<int> handleVisibleChange(const QSet<int>& visiblePages) const override;

private:
    static constexpr int BATCH_SIZE = 20;
    static constexpr int INITIAL_MARGIN = 15;
};

class LargeDocStrategy : public ThumbnailLoadStrategy
{
    Q_OBJECT

public:
    explicit LargeDocStrategy(int pageCount, QObject* parent = nullptr);

    LoadStrategyType type() const override { return LoadStrategyType::LARGE_DOC; }
    QVector<int> getInitialLoadPages(const QSet<int>& visiblePages) const override;
    QVector<QVector<int>> getBackgroundBatches() const override;
    QVector<int> handleVisibleChange(const QSet<int>& visiblePages) const override;

private:
    static constexpr int PAGE_WINDOW = 8;
    mutable QSet<int> m_loadedPages;
};

class StrategyFactory
{
public:
    static ThumbnailLoadStrategy* createStrategy(int pageCount, QObject* parent = nullptr);

private:
    static constexpr int SMALL_DOC_THRESHOLD = 50;
    static constexpr int MEDIUM_DOC_THRESHOLD = 400;
};

#endif // THUMBNAILLOADSTRATEGY_H
