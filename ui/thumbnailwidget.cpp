#include "thumbnailwidget.h"
#include "thumbnailmanagerv2.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QScrollBar>
#include <QDebug>
#include <QDateTime>

ThumbnailWidget::ThumbnailWidget(QWidget* parent)
    : QScrollArea(parent)
    , m_container(nullptr)
    , m_layout(nullptr)
    , m_thumbnailWidth(DEFAULT_THUMBNAIL_WIDTH)
    , m_currentPage(-1)
    , m_columnsPerRow(2)
    , m_scrollState(ScrollState::IDLE)
    , m_manager(nullptr)
{
    m_container = new QWidget(this);
    m_layout = new QGridLayout(m_container);
    m_layout->setSpacing(THUMBNAIL_SPACING);
    m_layout->setContentsMargins(
        THUMBNAIL_SPACING, THUMBNAIL_SPACING,
        THUMBNAIL_SPACING, THUMBNAIL_SPACING
        );

    setWidget(m_container);
    setWidgetResizable(true);

    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setSingleShot(true);
    m_throttleTimer->setInterval(30);
    connect(m_throttleTimer, &QTimer::timeout,
            this, &ThumbnailWidget::onScrollThrottle);

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(150);
    connect(m_debounceTimer, &QTimer::timeout,
            this, &ThumbnailWidget::onScrollDebounce);
}

ThumbnailWidget::~ThumbnailWidget()
{
    qDebug() << "ThumbnailWidget: Destructor called";
    clear();
    qDebug() << "ThumbnailWidget: Destructor finished";
}

void ThumbnailWidget::setThumbnailManager(ThumbnailManagerV2* manager)
{
    m_manager = manager;
}

bool ThumbnailWidget::isLargeLoadMode() {
    return m_manager->thumbnailLoadStrategy()->type() == LoadStrategyType::LARGE_DOC;
}

void ThumbnailWidget::initializeThumbnails(int pageCount)
{
    clear();

    if (pageCount <= 0) {
        qWarning() << "ThumbnailWidget: Invalid page count:" << pageCount;
        return;
    }

    qInfo() << "ThumbnailWidget: Initializing" << pageCount << "thumbnail placeholders";

    int availableWidth = viewport()->width() - 2 * THUMBNAIL_SPACING;
    int itemWidth = m_thumbnailWidth + 20;
    m_columnsPerRow = qMax(1, availableWidth / itemWidth);

    qDebug() << "ThumbnailWidget: viewport width =" << viewport()->width()
             << ", availableWidth =" << availableWidth
             << ", itemWidth =" << itemWidth
             << ", columns =" << m_columnsPerRow;

    for (int i = 0; i < pageCount; ++i) {
        auto* item = new ThumbnailItem(i, m_thumbnailWidth, m_container);
        item->setPlaceholder(tr("Page %1").arg(i + 1));

        connect(item, &ThumbnailItem::clicked,
                this, &ThumbnailWidget::onThumbnailClicked);

        int row = i / m_columnsPerRow;
        int col = i % m_columnsPerRow;
        m_layout->addWidget(item, row, col);

        m_thumbnailItems[i] = item;
    }

    qInfo() << "ThumbnailWidget: Created" << pageCount << "placeholder items";

    QTimer::singleShot(100, this, [this]() {
        m_container->adjustSize();
        m_layout->activate();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        QSet<int> initialVisible = getVisibleIndices(0);

        qDebug() << "ThumbnailWidget: Initial visible count =" << initialVisible.size();
        if (initialVisible.isEmpty()) {
            qWarning() << "ThumbnailWidget: No initial visible items found!";
            for (int i = 0; i < qMin(5, m_thumbnailItems.size()); ++i) {
                if (m_thumbnailItems.contains(i)) {
                    qDebug() << "  Widget" << i << "geometry:"
                             << m_thumbnailItems[i]->geometry();
                }
            }
        }

        emit initialVisibleReady(initialVisible);
    });
}

void ThumbnailWidget::clear()
{
    if (!m_layout)
        return;

    qDebug() << "ThumbnailWidget::clear() - Start";

    if (m_throttleTimer && m_throttleTimer->isActive()) {
        m_throttleTimer->stop();
    }
    if (m_debounceTimer && m_debounceTimer->isActive()) {
        m_debounceTimer->stop();
    }

    while (QLayoutItem* item = m_layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    m_thumbnailItems.clear();
    m_scrollHistory.clear();
    m_currentPage = -1;

    m_layout->invalidate();

    qDebug() << "ThumbnailWidget::clear() - Finished";
}

void ThumbnailWidget::highlightCurrentPage(int pageIndex)
{
    if (m_currentPage >= 0 && m_thumbnailItems.contains(m_currentPage)) {
        m_thumbnailItems[m_currentPage]->setHighlight(false);
    }

    m_currentPage = pageIndex;

    if (m_currentPage >= 0 && m_thumbnailItems.contains(m_currentPage)) {
        auto* item = m_thumbnailItems[m_currentPage];
        item->setHighlight(true);
        ensureWidgetVisible(item, 50, 50);
    }
}

void ThumbnailWidget::setThumbnailSize(int width)
{
    if (width < 80 || width > 400) {
        qWarning() << "ThumbnailWidget: Invalid thumbnail width:" << width;
        return;
    }

    if (m_thumbnailWidth != width) {
        m_thumbnailWidth = width;
    }
}

void ThumbnailWidget::onThumbnailLoaded(int pageIndex, const QImage& thumbnail)
{
    if (!m_thumbnailItems.contains(pageIndex)) {
        return;
    }

    ThumbnailItem* item = m_thumbnailItems[pageIndex];
    item->setThumbnail(thumbnail);
}

void ThumbnailWidget::scrollContentsBy(int dx, int dy)
{
    QScrollArea::scrollContentsBy(dx, dy);

    if(!isLargeLoadMode()) {
        m_scrollHistory.clear();
        return;
    }

    m_scrollState = detectScrollState();

    if (!m_throttleTimer->isActive()) {
        m_throttleTimer->start();
    }

    m_debounceTimer->start();
}

void ThumbnailWidget::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);

    int availableWidth = viewport()->width() - 2 * THUMBNAIL_SPACING;
    int itemWidth = m_thumbnailWidth + 20;
    int newColumns = qMax(1, availableWidth / itemWidth);

    if (newColumns != m_columnsPerRow) {
        m_columnsPerRow = newColumns;

        qDebug() << "ThumbnailWidget: Columns changed to" << m_columnsPerRow;

        for (int i = 0; i < m_thumbnailItems.size(); ++i) {
            int row = i / m_columnsPerRow;
            int col = i % m_columnsPerRow;

            QLayoutItem* layoutItem = m_layout->itemAtPosition(row, col);
            if (!layoutItem || layoutItem->widget() != m_thumbnailItems[i]) {
                m_layout->removeWidget(m_thumbnailItems[i]);
                m_layout->addWidget(m_thumbnailItems[i], row, col);
            }
        }

        if(isLargeLoadMode()) {
            m_throttleTimer->start();
        }
    }
}

void ThumbnailWidget::onScrollThrottle()
{
    if(!isLargeLoadMode()) {
        return;
    }

    if (m_manager && m_manager->shouldRespondToScroll()) {
        notifyVisibleRange();
    }
}

void ThumbnailWidget::onScrollDebounce()
{
    if(!isLargeLoadMode()) {
        return;
    }

    m_scrollState = ScrollState::IDLE;
    m_scrollHistory.clear();

    qDebug() << "ThumbnailWidget: Scroll stopped";

    if (!isVisible()) {
        qDebug() << "ThumbnailWidget: Not visible, ignoring scroll stop";
        return;
    }

    if (m_manager && !m_manager->shouldRespondToScroll()) {
        qDebug() << "ThumbnailWidget: Ignoring scroll stop during batch loading";
        return;
    }

    QSet<int> unloadedVisible = getUnloadedVisiblePages();

    qDebug() << "onScrollDebounce:" << unloadedVisible;

    if (!unloadedVisible.isEmpty()) {
        qInfo() << "ThumbnailWidget: Found" << unloadedVisible.size()
        << "unloaded visible pages after scroll stop, triggering sync load";

        emit syncLoadRequested(unloadedVisible);
    }
}

void ThumbnailWidget::onThumbnailClicked(int pageIndex)
{
    emit pageJumpRequested(pageIndex);
}

QSet<int> ThumbnailWidget::getVisibleIndices(int margin) const
{
    QSet<int> visible;
    if (m_thumbnailItems.isEmpty())
        return visible;

    QRect visibleRect = viewport()->rect();
    qDebug() << "getVisibleIndices visibleRect 1:" << visibleRect;
    visibleRect.adjust(0, -margin, 0, margin);
    qDebug() << "getVisibleIndices visibleRect 2:" << visibleRect;

    for (auto it = m_thumbnailItems.constBegin(); it != m_thumbnailItems.constEnd(); ++it) {
        int index = it.key();
        ThumbnailItem* item = it.value();
        if (!item)
            continue;

        QPoint topLeft = item->mapTo(viewport(), QPoint(0, 0));
        QRect itemRect(topLeft, item->size());

        if (itemRect.intersects(visibleRect)) {
            visible.insert(index);
        }
    }

    qDebug() << "ThumbnailWidget::getVisibleIndices" << visible;
    return visible;
}

QSet<int> ThumbnailWidget::getUnloadedVisiblePages() const
{
    QSet<int> unloaded;

    if (!isVisible()) {
        return unloaded;
    }

    if (m_thumbnailItems.isEmpty()) {
        return unloaded;
    }

    QSet<int> visible = getVisibleIndices(0);

    for (int pageIndex : visible) {
        if (m_thumbnailItems.contains(pageIndex)) {
            ThumbnailItem* item = m_thumbnailItems[pageIndex];
            qDebug() << "ThumbnailWidget: getUnloadedVisiblePages" << pageIndex << "hasImage=" << item->hasImage();
            if (!item->hasImage()) {
                unloaded.insert(pageIndex);
            }
        }
    }

    return unloaded;
}

void ThumbnailWidget::notifyVisibleRange()
{
    int margin = getPreloadMargin(m_scrollState);
    QSet<int> visible = getVisibleIndices(margin);

    emit visibleRangeChanged(visible, margin);
}

ScrollState ThumbnailWidget::detectScrollState()
{
    int currentPos = verticalScrollBar()->value();
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    m_scrollHistory.enqueue(qMakePair(currentPos, currentTime));

    while (!m_scrollHistory.isEmpty() &&
           currentTime - m_scrollHistory.first().second > 200) {
        m_scrollHistory.dequeue();
    }

    if (m_scrollHistory.size() < 2) {
        return ScrollState::IDLE;
    }

    auto [firstPos, firstTime] = m_scrollHistory.first();
    int distance = qAbs(currentPos - firstPos);
    qint64 duration = currentTime - firstTime;

    if (duration < 50) {
        return ScrollState::FLING;
    }

    float velocity = distance / (duration / 1000.0f);

    if (velocity < 500) {
        return ScrollState::IDLE;
    } else if (velocity < 1000) {
        return ScrollState::SLOW_SCROLL;
    } else if (velocity < 3000) {
        return ScrollState::FAST_SCROLL;
    } else {
        return ScrollState::FLING;
    }
}

int ThumbnailWidget::getPreloadMargin(ScrollState state) const
{
    switch (state) {
    case ScrollState::IDLE:
        return 1200;
    case ScrollState::SLOW_SCROLL:
        return 800;
    case ScrollState::FAST_SCROLL:
        return 400;
    case ScrollState::FLING:
        return 0;
    }
    return 800;
}

ThumbnailItem::ThumbnailItem(int pageIndex, int width, QWidget* parent)
    : QWidget(parent)
    , m_pageIndex(pageIndex)
    , m_width(width)
    , m_hasImage(false)
    , m_isHighlighted(false)
    , m_isHovered(false)
{
    m_height = static_cast<int>(width * ThumbnailWidget::A4_RATIO);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_imageContainer = new QWidget(this);
    auto* containerLayout = new QVBoxLayout(m_imageContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);

    m_imageLabel = new QLabel(m_imageContainer);
    m_imageLabel->setFixedSize(width, m_height);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setScaledContents(false);

    updateStyle();

    containerLayout->addWidget(m_imageLabel);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setColor(QColor(0, 0, 0, 40));
    shadow->setOffset(0, 2);
    m_imageContainer->setGraphicsEffect(shadow);

    m_pageLabel = new QLabel(tr("Page %1").arg(pageIndex + 1), this);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_pageLabel->font();
    font.setPointSize(9);
    m_pageLabel->setFont(font);
    m_pageLabel->setStyleSheet("QLabel { color: #666666; }");

    layout->addWidget(m_imageContainer);
    layout->addWidget(m_pageLabel);

    setFixedWidth(width + 16);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

void ThumbnailItem::setPlaceholder(const QString& text)
{
    m_hasImage = false;
    m_imageLabel->setText(text);
    m_imageLabel->setStyleSheet(
        "QLabel { "
        "    background-color: white; "
        "    border: 1px solid #E0E0E0; "
        "    border-radius: 4px; "
        "    color: #999999; "
        "}"
        );

    QFont font = m_imageLabel->font();
    font.setPointSize(9);
    m_imageLabel->setFont(font);
}

void ThumbnailItem::setThumbnail(const QImage& image)
{
    if (image.isNull()) {
        setError(tr("Load failed"));
        return;
    }

    m_hasImage = true;

    // 关键：按"物理像素"缩放，并保持 devicePixelRatio。
    // m_imageLabel->size() 是逻辑尺寸；乘上图自带的 dpr 得到目标物理像素。
    const qreal dpr = image.devicePixelRatio();   // Mac=2.0, Win=1.0
    const QSize targetPhysical = QSize(m_imageLabel->width(), m_imageLabel->height()) * dpr;

    QImage scaled = image.scaled(
        targetPhysical,                 // 目标用物理像素
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );
    scaled.setDevicePixelRatio(dpr);    // scaled() 会重置 dpr，这里补回来

    QPixmap pixmap = createRoundedPixmap(scaled);
    m_imageLabel->setPixmap(pixmap);
    m_imageLabel->setText(QString());

    updateStyle();
}

void ThumbnailItem::setError(const QString& error)
{
    m_hasImage = false;
    m_imageLabel->setText(error);
    m_imageLabel->setStyleSheet(
        "QLabel { "
        "    background-color: white; "
        "    border: 1px solid #E0E0E0; "
        "    border-radius: 4px; "
        "    color: #F44336; "
        "}"
        );

    QFont font = m_imageLabel->font();
    font.setPointSize(8);
    m_imageLabel->setFont(font);
}

void ThumbnailItem::setHighlight(bool highlight)
{
    m_isHighlighted = highlight;
    updateStyle();

    if (highlight) {
        m_pageLabel->setStyleSheet("QLabel { color: #2196F3; font-weight: bold; }");
    } else {
        m_pageLabel->setStyleSheet("QLabel { color: #666666; }");
    }
}

void ThumbnailItem::updateStyle()
{
    if (!m_hasImage) {
        return;
    }

    QString baseStyle = R"(
        QLabel {
            background-color: white;
            border-radius: 4px;
        }
    )";

    if (m_isHighlighted) {
        m_imageLabel->setStyleSheet(baseStyle +
                                    "QLabel { border: 3px solid #2196F3; }");
    } else if (m_isHovered) {
        m_imageLabel->setStyleSheet(baseStyle +
                                    "QLabel { border: 2px solid #64B5F6; }");
    } else {
        m_imageLabel->setStyleSheet(baseStyle +
                                    "QLabel { border: 1px solid #E0E0E0; }");
    }
}

QPixmap ThumbnailItem::createRoundedPixmap(const QImage& image)
{
    const qreal dpr = image.devicePixelRatio();

    QPixmap pixmap = QPixmap::fromImage(image);   // fromImage 会继承 image 的 dpr

    QPixmap rounded(pixmap.size());               // pixmap.size() 是物理像素
    rounded.setDevicePixelRatio(dpr);             // 关键：结果 pixmap 也要标 dpr
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    // 注意：painter 工作在逻辑坐标（因为 rounded 有 dpr），
    // 用 deviceIndependent 尺寸画圆角矩形，圆角半径才不会被放大。
    QRectF logicalRect(0, 0,
                       rounded.width() / dpr,
                       rounded.height() / dpr);
    path.addRoundedRect(logicalRect, 4, 4);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, pixmap);

    return rounded;
}

void ThumbnailItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_pageIndex);
    }
    QWidget::mousePressEvent(event);
}

void ThumbnailItem::enterEvent(QEnterEvent* event)
{
    m_isHovered = true;
    updateStyle();

    if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(
            m_imageContainer->graphicsEffect())) {
        shadow->setBlurRadius(16);
        shadow->setColor(QColor(0, 0, 0, 60));
        shadow->setOffset(0, 4);
    }

    QWidget::enterEvent(event);
}

void ThumbnailItem::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    updateStyle();

    if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(
            m_imageContainer->graphicsEffect())) {
        shadow->setBlurRadius(12);
        shadow->setColor(QColor(0, 0, 0, 40));
        shadow->setOffset(0, 2);
    }

    QWidget::leaveEvent(event);
}
