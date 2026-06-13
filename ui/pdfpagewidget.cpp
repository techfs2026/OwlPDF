#include "pdfpagewidget.h"
#include "pdfdocumentsession.h"
#include "pdfdocumentstate.h"
#include "perthreadmupdfrenderer.h"
#include "pagecachemanager.h"
#include "pdfinteractionhandler.h"
#include "pdfannotationhandler.h"
#include "annotationmanager.h"
#include "textselector.h"
#include "linkmanager.h"
#include "ocrmanager.h"
#include "appconfig.h"

#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QMouseEvent>
#include <QDebug>
#include <functional>

namespace {
// 渲染图带有 devicePixelRatio 标记后，width()/height() 仍返回物理像素。
// 控件布局、命中测试、绘制定位一律使用逻辑尺寸（物理像素 / dpr）。
inline QSize logicalImageSize(const QImage& img)
{
    return img.deviceIndependentSize().toSize();
}
}

PDFPageWidget::PDFPageWidget(PDFDocumentSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_renderer(nullptr)
    , m_cacheManager(nullptr)
    , m_isTextSelecting(false)
    , m_ocrHoverEnabled(false)
{
    if (!m_session) {
        qCritical() << "PDFPageWidget: session is null!";
        return;
    }

    m_renderer = m_session->renderer();
    m_cacheManager = m_session->pageCache();

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    if (AnnotationManager* am = m_session->annotationManager()) {
        connect(am, &AnnotationManager::annotationsChanged,
                this, [this](int) { update(); });
    }
    if (PDFAnnotationHandler* ah = m_session->annotationHandler()) {
        connect(ah, &PDFAnnotationHandler::toolChanged,
                this, &PDFPageWidget::onAnnotationToolChanged);
    }

    setupOCRHover();
}

PDFPageWidget::~PDFPageWidget()
{
}


void PDFPageWidget::setDisplayImages(const QImage& primaryImage, const QImage& secondaryImage)
{
    m_currentImage = primaryImage;
    m_secondImage = secondaryImage;

    QSize targetSize = sizeHint();
    resize(targetSize);

    update();
}

void PDFPageWidget::refreshVisiblePages()
{
    emit visibleAreaChanged();
}

void PDFPageWidget::setTextSelectionMode(bool enabled)
{
    m_isTextSelecting = enabled;
    if (!enabled) {
        setCursor(Qt::IBeamCursor);
    }
}

void PDFPageWidget::clearHighlights()
{
    update();
}


QPointF PDFPageWidget::screenToPageCoord(const QPoint& screenPos, int pageX, int pageY) const
{
    return QPointF(screenPos.x() - pageX, screenPos.y() - pageY);
}

int PDFPageWidget::getPageAtPos(const QPoint& pos, int* pageX, int* pageY) const
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return -1;
    }

    const int margin = AppConfig::PAGE_MARGIN;
    const PDFDocumentState* state = m_session->state();

    if (state->isContinuousScroll() && !state->pageYPositions().isEmpty()) {
        const QVector<int>& positions = state->pageYPositions();
        const QVector<int>& heights = state->pageHeights();

        for (int i = 0; i < positions.size(); ++i) {
            int top = positions[i] + margin;
            int bottom = top + heights[i];

            if (pos.y() >= top && pos.y() <= bottom) {
                double actualZoom = state->currentZoom();
                QSizeF pageSize = m_renderer->pageSize(i);
                if (state->currentRotation() == 90 || state->currentRotation() == 270) {
                    pageSize.transpose();
                }
                int pageWidth = qRound(pageSize.width() * actualZoom);
                int left = (width() - pageWidth) / 2;
                int right = left + pageWidth;

                if (pos.x() >= left && pos.x() <= right) {
                    if (pageX) *pageX = left;
                    if (pageY) *pageY = top;
                    return i;
                }
            }
        }
        return -1;
    }
    else {
        int currentPage = state->currentPage();
        QSize firstSize = logicalImageSize(m_currentImage);
        int contentX = (width() - firstSize.width()) / 2;
        int contentY = (height() - firstSize.height()) / 2;

        QRect firstPageRect(QPoint(contentX, contentY), firstSize);
        if (firstPageRect.contains(pos)) {
            if (pageX) *pageX = contentX;
            if (pageY) *pageY = contentY;
            return currentPage;
        }

        if (state->currentDisplayMode() == PageDisplayMode::DoublePage && !m_secondImage.isNull()) {
            QSize secondSize = logicalImageSize(m_secondImage);
            int secondX = contentX + firstSize.width() + AppConfig::DOUBLE_PAGE_SPACING;
            int maxHeight = qMax(firstSize.height(), secondSize.height());
            int secondY = contentY + (maxHeight - secondSize.height()) / 2;

            QRect secondPageRect(QPoint(secondX, secondY), secondSize);
            if (secondPageRect.contains(pos)) {
                if (pageX) *pageX = secondX;
                if (pageY) *pageY = secondY;
                return currentPage + 1;
            }
        }
    }

    return -1;
}

QScrollArea* PDFPageWidget::getScrollArea() const
{
    QWidget* parentWgt = parentWidget();
    if (parentWgt) {
        return qobject_cast<QScrollArea*>(parentWgt->parentWidget());
    }
    return nullptr;
}

QSize PDFPageWidget::getViewportSize() const
{
    QScrollArea* scrollArea = getScrollArea();
    if (scrollArea) {
        return scrollArea->viewport()->size();
    }
    return size();
}

QSize PDFPageWidget::calculateRequiredSize() const
{
    return sizeHint();
}

QString PDFPageWidget::getCacheStatistics() const
{
    if (!m_cacheManager) {
        return "Cache: Not initialized";
    }
    return m_cacheManager->getStatistics();
}


QSize PDFPageWidget::sizeHint() const
{
    const PDFDocumentState* state = m_session->state();

    if (m_currentImage.isNull() && state->pageYPositions().isEmpty()) {
        QSize viewportSize = getViewportSize();
        if (viewportSize.isValid() && viewportSize.width() > 0 && viewportSize.height() > 0) {
            return viewportSize;
        }
        return QSize(800, 600);
    }

    const int margin = AppConfig::PAGE_MARGIN;

    if (state->isContinuousScroll() && !state->pageYPositions().isEmpty()) {
        int maxWidth = 0;

        if (m_renderer && m_renderer->isDocumentLoaded()) {
            QSizeF pageSize = m_renderer->pageSize(0);
            if (state->currentRotation() == 90 || state->currentRotation() == 270) {
                pageSize.transpose();
            }
            maxWidth = qRound(pageSize.width() * state->currentZoom());
        }

        const QVector<int>& positions = state->pageYPositions();
        const QVector<int>& heights = state->pageHeights();
        int totalHeight = positions.last() + heights.last();
        return QSize(maxWidth + 2 * margin, totalHeight + 2 * margin);
    }

    QSize firstSize = logicalImageSize(m_currentImage);
    int contentWidth = firstSize.width();
    int contentHeight = firstSize.height();

    if (state->currentDisplayMode() == PageDisplayMode::DoublePage && !m_secondImage.isNull()) {
        QSize secondSize = logicalImageSize(m_secondImage);
        contentWidth = firstSize.width() + secondSize.width() + AppConfig::DOUBLE_PAGE_SPACING;
        contentHeight = qMax(firstSize.height(), secondSize.height());
    }

    return QSize(contentWidth + 2 * margin, contentHeight + 2 * margin);
}

void PDFPageWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const PDFDocumentState* state = m_session->state();

    if (state->isContinuousScroll() && !state->pageYPositions().isEmpty()) {
        paintContinuousMode(painter, event->rect());
        drawEraserCursor(painter);
        return;
    }

    if (m_currentImage.isNull()) {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(12);
        painter.setFont(font);

        QScrollArea* scrollArea = getScrollArea();
        if (scrollArea && scrollArea->viewport()) {
            painter.drawText(scrollArea->viewport()->rect(), Qt::AlignCenter, tr("No document loaded"));
        }
        return;
    }

    if (state->currentDisplayMode() == PageDisplayMode::SinglePage || m_secondImage.isNull()) {
        paintSinglePageMode(painter);
    } else {
        paintDoublePageMode(painter);
    }

    drawEraserCursor(painter);
}

void PDFPageWidget::paintSinglePageMode(QPainter& painter)
{
    QSize imgSize = logicalImageSize(m_currentImage);
    int x = (width() - imgSize.width()) / 2;
    int y = (height() - imgSize.height()) / 2;

    drawPageImage(painter, m_currentImage, x, y);

    const PDFDocumentState* state = m_session->state();
    drawOverlays(painter, state->currentPage(), x, y, state->currentZoom());
}

void PDFPageWidget::paintDoublePageMode(QPainter& painter)
{
    QSize firstSize = logicalImageSize(m_currentImage);
    QSize secondSize = logicalImageSize(m_secondImage);
    int totalWidth = firstSize.width() + secondSize.width() + AppConfig::DOUBLE_PAGE_SPACING;
    int maxHeight = qMax(firstSize.height(), secondSize.height());

    int startX = (width() - totalWidth) / 2;
    int startY = (height() - maxHeight) / 2;

    const PDFDocumentState* state = m_session->state();
    int currentPage = state->currentPage();
    double actualZoom = state->currentZoom();

    int x1 = startX;
    int y1 = startY + (maxHeight - firstSize.height()) / 2;
    drawPageImage(painter, m_currentImage, x1, y1);
    drawOverlays(painter, currentPage, x1, y1, actualZoom);

    int x2 = startX + firstSize.width() + AppConfig::DOUBLE_PAGE_SPACING;
    int y2 = startY + (maxHeight - secondSize.height()) / 2;
    drawPageImage(painter, m_secondImage, x2, y2);

    if (!m_secondImage.isNull()) {
        int nextPage = currentPage + 1;
        if (nextPage < m_renderer->pageCount()) {
            drawOverlays(painter, nextPage, x2, y2, actualZoom);
        }
    }
}

void PDFPageWidget::paintContinuousMode(QPainter& painter, const QRect& visibleRect)
{
    const int margin = AppConfig::PAGE_MARGIN;
    const PDFDocumentState* state = m_session->state();
    double actualZoom = state->currentZoom();
    int rotation = state->currentRotation();
    double dpr = devicePixelRatioF();

    QList<PageCacheKey> cachedKeys = m_cacheManager->cachedKeys();

    for (const PageCacheKey& key : cachedKeys) {
        int pageIndex = key.pageIndex;

        if (qAbs(key.zoom - actualZoom) >= 0.001 || key.rotation != rotation
            || qAbs(key.dpr - dpr) >= 0.001) {
            continue;
        }

        if (pageIndex >= state->pageYPositions().size()) {
            continue;
        }

        QImage pageImage = m_cacheManager->getPage(pageIndex, actualZoom, rotation, dpr);
        if (pageImage.isNull()) {
            continue;
        }

        QSize imgSize = logicalImageSize(pageImage);
        int pageY = state->pageYPositions()[pageIndex] + margin;
        int pageX = (width() - imgSize.width()) / 2;

        int pageBottom = pageY + imgSize.height();
        if (pageBottom >= visibleRect.top() && pageY <= visibleRect.bottom()) {
            drawPageImage(painter, pageImage, pageX, pageY);
            drawOverlays(painter, pageIndex, pageX, pageY, actualZoom);
        }
    }

    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);

    const QVector<int>& positions = state->pageYPositions();
    const QVector<int>& heights = state->pageHeights();

    for (int i = 0; i < positions.size(); ++i) {
        if (!m_cacheManager->contains(i, actualZoom, rotation, dpr)) {
            int pageY = positions[i] + margin;
            int pageHeight = heights[i];

            if (pageY + pageHeight >= visibleRect.top() && pageY <= visibleRect.bottom()) {
                QRect placeholderRect(margin, pageY, width() - 2 * margin, pageHeight);
                drawPagePlaceholder(painter, placeholderRect, i);
            }
        }
    }
}

void PDFPageWidget::drawPageImage(QPainter& painter, const QImage& image, int x, int y)
{
    // 阴影按逻辑尺寸绘制；image.rect() 是物理像素，Retina 下会偏大一倍。
    QRect shadowRect(QPoint(x + AppConfig::SHADOW_OFFSET, y + AppConfig::SHADOW_OFFSET),
                     logicalImageSize(image));
    painter.fillRect(shadowRect, QColor(0, 0, 0, 100));

    // image 已标记 devicePixelRatio，drawImage 会自动按逻辑尺寸绘制。
    painter.drawImage(x, y, image);
}

void PDFPageWidget::drawPagePlaceholder(QPainter& painter, const QRect& rect, int pageIndex)
{
    painter.fillRect(rect, QColor(80, 80, 80));
    painter.drawText(rect, Qt::AlignCenter, tr("Loading page %1...").arg(pageIndex + 1));
}

void PDFPageWidget::drawOverlays(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom)
{
    drawSearchHighlights(painter, pageIndex, pageX, pageY, zoom);
    drawTextSelection(painter, pageIndex, pageX, pageY, zoom);

    // 不论何种 PDF 都不画链接外框（外框干扰阅读）；链接的点击跳转/悬停提示
    // 仍由 state->linksVisible() 控制，绘制层不再涉及链接。
    drawAnnotations(painter, pageIndex, pageX, pageY, zoom);
}

void PDFPageWidget::drawSearchHighlights(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom)
{
    PDFInteractionHandler* handler = m_session->interactionHandler();
    if (!handler) return;

    QVector<SearchResult> results = handler->getPageSearchResults(pageIndex);
    if (results.isEmpty()) return;

    const PDFDocumentState* state = m_session->state();
    int currentMatchIndex = state->searchCurrentMatchIndex();
    int totalMatches = state->searchTotalMatches();

    int globalIndex = 0;
    for (int pg = 0; pg < pageIndex; pg++) {
        QVector<SearchResult> prevResults = handler->getPageSearchResults(pg);
        globalIndex += prevResults.size();
    }

    for (int i = 0; i < results.size(); ++i) {
        const SearchResult& result = results[i];
        bool isCurrent = (globalIndex + i == currentMatchIndex);

        for (const QRectF& quad : result.quads) {
            QRectF scaledQuad(quad.x() * zoom, quad.y() * zoom, quad.width() * zoom, quad.height() * zoom);
            scaledQuad.translate(pageX, pageY);

            if (isCurrent) {
                // 当前项：高饱和橙 + 深橙描边，与其它命中明显区分
                painter.fillRect(scaledQuad, QColor(255, 122, 0, 180));
                painter.setPen(QPen(QColor(214, 92, 0), 2));
                painter.drawRect(scaledQuad);
            } else {
                // 其它命中：淡黄
                painter.fillRect(scaledQuad, QColor(255, 235, 59, 90));
            }
        }
    }
}

void PDFPageWidget::drawLinkAreas(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom)
{
    PDFInteractionHandler* handler = m_session->interactionHandler();
    if (!handler) return;

    QVector<PDFLink> links = handler->loadPageLinks(pageIndex);
    if (links.isEmpty()) return;

    for (const PDFLink& link : links) {
        QRectF scaledRect(link.rect.x() * zoom, link.rect.y() * zoom,
                          link.rect.width() * zoom, link.rect.height() * zoom);
        scaledRect.translate(pageX, pageY);

        painter.fillRect(scaledRect, QColor(0, 120, 215, 30));
        painter.setPen(QPen(QColor(0, 120, 215, 100), 1, Qt::DashLine));
        painter.drawRect(scaledRect);
    }
}

void PDFPageWidget::drawTextSelection(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom)
{
    PDFInteractionHandler* handler = m_session->interactionHandler();
    if (!handler) return;

    const TextSelection& selection = handler->getCurrentTextSelection();
    if (selection.pageIndex != pageIndex) return;

    QColor highlightColor;
    switch (selection.mode) {
    case SelectionMode::Word:
        highlightColor = QColor(100, 150, 255, 100);
        break;
    case SelectionMode::Line:
        highlightColor = QColor(150, 200, 255, 90);
        break;
    default:
        highlightColor = QColor(0, 120, 215, 80);
        break;
    }

    painter.save();
    painter.setBrush(highlightColor);
    painter.setPen(Qt::NoPen);

    for (const QRectF& rect : selection.highlightRects) {
        QRectF scaledRect(rect.x() * zoom + pageX, rect.y() * zoom + pageY,
                          rect.width() * zoom, rect.height() * zoom);
        painter.drawRect(scaledRect);
    }

    painter.restore();
}

// ============================================================
// 批注（钢笔/橡皮）
// ============================================================

namespace {
// 把一条笔迹画到 painter：坐标 未旋转pt → 旋转显示pt → ×zoom + 页面偏移。
void paintStroke(QPainter& painter, const InkStroke& stroke, double zoom,
                 int pageX, int pageY,
                 const std::function<QPointF(const QPointF&)>& toDisplay)
{
    if (stroke.points.isEmpty()) {
        return;
    }

    auto toScreen = [&](const QPointF& p) {
        const QPointF d = toDisplay(p);
        return QPointF(d.x() * zoom + pageX, d.y() * zoom + pageY);
    };

    QPen pen(stroke.color, qMax(0.5, stroke.width * zoom));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (stroke.points.size() == 1) {
        // 单点 → 画一个圆点
        const QPointF c = toScreen(stroke.points.first());
        const qreal r = qMax(0.5, stroke.width * zoom / 2.0);
        painter.setBrush(stroke.color);
        painter.drawEllipse(c, r, r);
        return;
    }

    QPainterPath path(toScreen(stroke.points.first()));
    for (int i = 1; i < stroke.points.size(); ++i) {
        path.lineTo(toScreen(stroke.points[i]));
    }
    painter.drawPath(path);
}
}

void PDFPageWidget::drawAnnotations(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom)
{
    AnnotationManager* manager = m_session->annotationManager();
    if (!manager) return;

    auto toDisplay = [this, pageIndex](const QPointF& p) {
        return unrotatedToDisplay(p, pageIndex);
    };

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const InkStroke& stroke : manager->strokesForPage(pageIndex)) {
        paintStroke(painter, stroke, zoom, pageX, pageY, toDisplay);
    }

    // 正在画的当前笔
    PDFAnnotationHandler* handler = m_session->annotationHandler();
    if (handler && handler->isDrawing()) {
        const InkStroke& cur = handler->currentStroke();
        if (cur.pageIndex == pageIndex) {
            paintStroke(painter, cur, zoom, pageX, pageY, toDisplay);
        }
    }

    painter.restore();
}

void PDFPageWidget::drawEraserCursor(QPainter& painter)
{
    PDFAnnotationHandler* handler = m_session->annotationHandler();
    if (!handler || handler->tool() != AnnotTool::Eraser) {
        return;
    }
    if (m_lastHoverPos.isNull() || !rect().contains(m_lastHoverPos)) {
        return;
    }

    const double zoom = m_session->state()->currentZoom();
    const qreal r = qMax(2.0, handler->eraserRadius() * zoom);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(0, 0, 0, 20));
    painter.setPen(QPen(QColor(80, 80, 80, 160), 1));
    painter.drawEllipse(QPointF(m_lastHoverPos), r, r);
    painter.restore();
}

AnnotTool PDFPageWidget::annotTool() const
{
    PDFAnnotationHandler* handler = m_session ? m_session->annotationHandler() : nullptr;
    return handler ? handler->tool() : AnnotTool::None;
}

void PDFPageWidget::onAnnotationToolChanged(AnnotTool tool)
{
    switch (tool) {
    case AnnotTool::Pen:
        setCursor(Qt::CrossCursor);
        break;
    case AnnotTool::Eraser:
        setCursor(Qt::BlankCursor);   // 用绘制的圆圈代替光标
        break;
    case AnnotTool::None: {
        const PDFDocumentState* state = m_session->state();
        setCursor(state && state->isTextPDF() ? Qt::IBeamCursor : Qt::ArrowCursor);
        break;
    }
    }
    update();
}

QPointF PDFPageWidget::unrotatedToDisplay(const QPointF& pt, int pageIndex) const
{
    const int rot = m_session->state()->currentRotation();
    if (rot == 0 || !m_renderer) {
        return pt;
    }
    const QSizeF sz = m_renderer->pageSize(pageIndex);
    const qreal W = sz.width();
    const qreal H = sz.height();
    const qreal x = pt.x();
    const qreal y = pt.y();
    switch (rot) {
    case 90:  return QPointF(H - y, x);
    case 180: return QPointF(W - x, H - y);
    case 270: return QPointF(y, W - x);
    default:  return pt;
    }
}

QPointF PDFPageWidget::displayToUnrotated(const QPointF& d, int pageIndex) const
{
    const int rot = m_session->state()->currentRotation();
    if (rot == 0 || !m_renderer) {
        return d;
    }
    const QSizeF sz = m_renderer->pageSize(pageIndex);
    const qreal W = sz.width();
    const qreal H = sz.height();
    const qreal X = d.x();
    const qreal Y = d.y();
    switch (rot) {
    case 90:  return QPointF(Y, H - X);
    case 180: return QPointF(W - X, H - Y);
    case 270: return QPointF(W - Y, X);
    default:  return d;
    }
}

int PDFPageWidget::posToPageStroke(const QPoint& pos, QPointF* unrotatedPt) const
{
    int pageX = 0, pageY = 0;
    int pageIndex = getPageAtPos(pos, &pageX, &pageY);
    if (pageIndex < 0) {
        return -1;
    }
    const double zoom = m_session->state()->currentZoom();
    // 屏幕 → 显示页面坐标(pt)
    const QPointF displayPt = screenToPageCoord(pos, pageX, pageY) / qMax(0.0001, zoom);
    if (unrotatedPt) {
        *unrotatedPt = displayToUnrotated(displayPt, pageIndex);
    }
    return pageIndex;
}

void PDFPageWidget::triggerOCRAtCurrentPosition()
{
    if (!OCRManager::instance().isOCRHoverEnabled()) {
        qDebug() << "OCR hover not enabled globally";
        return;
    }

    if (!m_ocrHoverEnabled) {
        qDebug() << "OCR hover not enabled in widget";
        return;
    }

    const PDFDocumentState* state = m_session->state();
    if (!state->isDocumentLoaded()) {
        qDebug() << "No document loaded";
        return;
    }

    QPoint hoverPos = m_lastHoverPos;
    if (hoverPos.isNull() || !rect().contains(hoverPos)) {
        hoverPos = mapFromGlobal(QCursor::pos());

        if (!rect().contains(hoverPos)) {
            hoverPos = rect().center();
        }
    }

    int pageX, pageY;
    int pageIndex = getPageAtPos(hoverPos, &pageX, &pageY);

    if (pageIndex < 0) {
        qDebug() << "Position not on any page:" << hoverPos;
        return;
    }

    QImage image = extractHoverRegion(hoverPos);
    if (!image.isNull()) {
        QRect regionRect = calculateHoverRect(hoverPos);
        qInfo() << "Manual OCR triggered at position:" << hoverPos;
        emit ocrHoverTriggered(image, regionRect, hoverPos);
    } else {
        qDebug() << "Failed to extract hover region";
    }
}

void PDFPageWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_lastHoverPos = event->pos();

    // 批注工具优先：钢笔/橡皮接管移动
    const AnnotTool tool = annotTool();
    if (tool != AnnotTool::None) {
        PDFAnnotationHandler* handler = m_session->annotationHandler();
        if (m_annotMouseDown && (event->buttons() & Qt::LeftButton)) {
            QPointF pt;
            int pageIndex = posToPageStroke(event->pos(), &pt);
            if (pageIndex >= 0) {
                if (tool == AnnotTool::Pen) {
                    // 仅在同一页内延笔，跨页暂停（v1 简化）
                    if (handler->isDrawing() && handler->currentStroke().pageIndex == pageIndex) {
                        handler->extendStroke(pt);
                    }
                } else {
                    handler->eraseAt(pageIndex, pt);
                }
            }
            update();
        } else if (tool == AnnotTool::Eraser) {
            update();   // 刷新橡皮光标圆圈
        }
        event->accept();
        return;
    }

    if (m_ocrHoverEnabled) {
        event->accept();
        return;
    }

    const PDFDocumentState* state = m_session->state();

    if (m_isTextSelecting) {
        int pageX, pageY;
        int pageIndex = getPageAtPos(event->pos(), &pageX, &pageY);

        if (pageIndex >= 0) {
            QPointF pagePos = screenToPageCoord(event->pos(), pageX, pageY);
            emit textSelectionDragging(pageIndex, pagePos);
        }

        event->accept();
        return;
    }

    int pageX, pageY;
    int pageIndex = getPageAtPos(event->pos(), &pageX, &pageY);

    if (pageIndex < 0) {
        emit mouseLeftAllPages();
        if (cursor().shape() != Qt::ArrowCursor) {
            setCursor(Qt::ArrowCursor);
        }
        QWidget::mouseMoveEvent(event);
        return;
    }

    QPointF pagePos = screenToPageCoord(event->pos(), pageX, pageY);
    emit mouseMovedOnPage(pageIndex, pagePos);

    QWidget::mouseMoveEvent(event);
}

void PDFPageWidget::mousePressEvent(QMouseEvent* event)
{
    // 批注工具优先
    const AnnotTool tool = annotTool();
    if (tool != AnnotTool::None) {
        if (event->button() == Qt::LeftButton) {
            QPointF pt;
            int pageIndex = posToPageStroke(event->pos(), &pt);
            if (pageIndex >= 0) {
                PDFAnnotationHandler* handler = m_session->annotationHandler();
                if (tool == AnnotTool::Pen) {
                    handler->beginStroke(pageIndex, pt);
                } else {
                    handler->eraseAt(pageIndex, pt);
                }
                m_annotMouseDown = true;
                update();
            }
        }
        event->accept();
        return;
    }

    int pageX, pageY;
    int pageIndex = getPageAtPos(event->pos(), &pageX, &pageY);

    if (pageIndex < 0) {
        QWidget::mousePressEvent(event);
        return;
    }

    QPointF pagePos = screenToPageCoord(event->pos(), pageX, pageY);

    emit pageClicked(pageIndex, pagePos, event->button(), event->modifiers());

    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
    }

    event->accept();
}

void PDFPageWidget::mouseReleaseEvent(QMouseEvent* event)
{
    const AnnotTool tool = annotTool();
    if (tool != AnnotTool::None) {
        if (event->button() == Qt::LeftButton) {
            if (tool == AnnotTool::Pen) {
                m_session->annotationHandler()->endStroke();
            }
            m_annotMouseDown = false;
            update();
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_isTextSelecting) {
        m_isTextSelecting = false;
        emit textSelectionEnded();
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        int pageX, pageY;
        int pageIndex = getPageAtPos(event->pos(), &pageX, &pageY);

        if (pageIndex >= 0) {
            QPointF pagePos = screenToPageCoord(event->pos(), pageX, pageY);
            emit contextMenuRequested(pageIndex, pagePos, event->globalPosition().toPoint());
        }
    }

    QWidget::mouseReleaseEvent(event);
}


void PDFPageWidget::setupOCRHover()
{
    m_hoverTimer.setSingleShot(true);
    m_hoverTimer.setInterval(AppConfig::instance().ocrDebounceDelay());
}

void PDFPageWidget::setOCRHoverEnabled(bool enabled)
{
    m_ocrHoverEnabled = enabled;

    if (!enabled) {
        m_hoverTimer.stop();
    }

    if (enabled) {
        setCursor(Qt::CrossCursor);
    } else {
        const PDFDocumentState* state = m_session->state();
        if (state && state->isTextPDF()) {
            setCursor(Qt::IBeamCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }

    qInfo() << "OCR hover enabled changed to:" << enabled;
}

QImage PDFPageWidget::extractHoverRegion(const QPoint& pos)
{
    const PDFDocumentState* state = m_session->state();
    if (!state->isDocumentLoaded()) {
        return QImage();
    }

    int pageX, pageY;
    int pageIndex = getPageAtPos(pos, &pageX, &pageY);

    if (pageIndex < 0) {
        return QImage();
    }

    QRect hoverRect = calculateHoverRect(pos);

    double zoom = state->currentZoom();
    int rotation = state->currentRotation();
    double dpr = devicePixelRatioF();

    QImage pageImage = m_cacheManager->getPage(pageIndex, zoom, rotation, dpr);

    if (pageImage.isNull()) {
        auto result = m_renderer->renderPage(pageIndex, zoom, rotation, RenderScene::Page, dpr);
        if (!result.success) {
            return QImage();
        }
        pageImage = result.image;
    }

    // hoverRect 是逻辑坐标，pageImage 含物理像素；按图像 dpr 换算到物理矩形。
    qreal imgDpr = pageImage.devicePixelRatio();
    QRectF logicalRect = QRectF(hoverRect).translated(-pageX, -pageY);
    QRect imageRect = QRectF(logicalRect.x() * imgDpr, logicalRect.y() * imgDpr,
                             logicalRect.width() * imgDpr, logicalRect.height() * imgDpr).toRect();
    imageRect = imageRect.intersected(pageImage.rect());

    if (imageRect.isEmpty()) {
        return QImage();
    }

    // 裁出的子图交给 OCR 作原始像素使用，dpr 归 1.0。
    QImage region = pageImage.copy(imageRect);
    region.setDevicePixelRatio(1.0);
    return region;
}

QRect PDFPageWidget::calculateHoverRect(const QPoint& centerPos)
{
    int baseSize = AppConfig::instance().ocrHoverRegionSize();


    int actualSize = baseSize;

    QRect rect(
        centerPos.x() - actualSize / 2,
        centerPos.y() - actualSize / 2,
        actualSize,
        actualSize
        );

    rect = rect.intersected(this->rect());

    return rect;
}
