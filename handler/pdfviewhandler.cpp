#include "pdfviewhandler.h"
#include "perthreadmupdfrenderer.h"
#include "pdfdocumentstate.h"
#include "appconfig.h"
#include <QDebug>
#include <algorithm>

PDFViewHandler::PDFViewHandler(PerThreadMuPDFRenderer* renderer,
                               const PDFDocumentState* state,
                               QObject* parent)
    : QObject(parent)
    , m_renderer(renderer)
    , m_state(state)
{
}

PDFViewHandler::~PDFViewHandler()
{
}

void PDFViewHandler::requestGoToPage(int pageIndex, bool adjustForDoublePageMode)
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return;
    }

    int pageCount = m_renderer->pageCount();
    if (pageIndex < 0 || pageIndex >= pageCount) {
        return;
    }

    if (adjustForDoublePageMode &&
        m_state->currentDisplayMode() == PageDisplayMode::DoublePage) {
        pageIndex = getDoublePageStartIndex(pageIndex);
    }

    if (m_state->currentPage() != pageIndex) {
        emit pageNavigationCompleted(pageIndex);
    }
}

void PDFViewHandler::requestPreviousPage()
{
    int prevPage = getPreviousPageIndex(m_state->currentDisplayMode(),
                                        m_state->isContinuousScroll(),
                                        m_state->currentPage());
    if (prevPage >= 0) {
        emit pageNavigationCompleted(prevPage);
    }
}

void PDFViewHandler::requestNextPage()
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return;
    }

    int nextPage = getNextPageIndex(m_state->currentDisplayMode(),
                                    m_state->isContinuousScroll(),
                                    m_state->currentPage());

    if (nextPage < m_state->pageCount()) {
        emit pageNavigationCompleted(nextPage);
    }
}

void PDFViewHandler::requestFirstPage()
{
    emit pageNavigationCompleted(0);
}

void PDFViewHandler::requestLastPage()
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return;
    }

    int lastPage = m_state->pageCount() - 1;

    if (m_state->currentDisplayMode() == PageDisplayMode::DoublePage) {
        lastPage = getDoublePageStartIndex(lastPage);
    }

    emit pageNavigationCompleted(lastPage);
}

int PDFViewHandler::getPreviousPageIndex(PageDisplayMode displayMode,
                                         bool continuousScroll,
                                         int currentPage) const
{
    if (displayMode == PageDisplayMode::DoublePage && !continuousScroll) {
        return currentPage - 2;
    }
    return currentPage - 1;
}

int PDFViewHandler::getNextPageIndex(PageDisplayMode displayMode,
                                     bool continuousScroll,
                                     int currentPage) const
{
    if (displayMode == PageDisplayMode::DoublePage && !continuousScroll) {
        return currentPage + 2;
    }
    return currentPage + 1;
}

int PDFViewHandler::getDoublePageStartIndex(int pageIndex) const
{
    return (pageIndex / 2) * 2;
}

void PDFViewHandler::requestSetZoom(double zoom)
{
    zoom = clampZoom(zoom);
    emit zoomSettingCompleted(zoom, ZoomMode::Custom);
}

void PDFViewHandler::requestSetZoomMode(ZoomMode mode)
{
    emit zoomSettingCompleted(-1.0, mode);
}

void PDFViewHandler::requestZoomIn()
{
    double newZoom = clampZoom(m_state->currentZoom() + ZOOM_STEP);
    emit zoomSettingCompleted(newZoom, ZoomMode::Custom);
}

void PDFViewHandler::requestZoomOut()
{
    double newZoom = clampZoom(m_state->currentZoom() - ZOOM_STEP);
    emit zoomSettingCompleted(newZoom, ZoomMode::Custom);
}

double PDFViewHandler::calculateActualZoom(const QSize& viewportSize,
                                           ZoomMode zoomMode,
                                           double customZoom,
                                           int currentPage,
                                           PageDisplayMode displayMode,
                                           int rotation,
                                           bool isContinuousScroll,
                                           int verticalScrollBarWidth) const
{
    double actualZoom = customZoom;

    if (zoomMode == ZoomMode::FitPage) {
        actualZoom = calculateFitPageZoom(viewportSize, currentPage, rotation);
    } else if (zoomMode == ZoomMode::FitWidth) {
        actualZoom = calculateFitWidthZoom(viewportSize, currentPage, displayMode,
                                           rotation, m_renderer->pageCount(),
                                           isContinuousScroll, verticalScrollBarWidth);
    }

    return clampZoom(actualZoom);
}

double PDFViewHandler::calculateFitPageZoom(const QSize& viewportSize,
                                            int currentPage,
                                            int rotation) const
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return DEFAULT_ZOOM;
    }

    QSizeF pageSize = m_renderer->pageSize(currentPage);
    if (pageSize.isEmpty()) {
        return DEFAULT_ZOOM;
    }

    if (rotation == 90 || rotation == 270) {
        pageSize.transpose();
    }

    const int margin = AppConfig::PAGE_MARGIN;
    int availableWidth = viewportSize.width() - 2 * margin;
    int availableHeight = viewportSize.height() - 2 * margin;

    if (availableWidth <= 0 || availableHeight <= 0) {
        return DEFAULT_ZOOM;
    }

    double widthZoom = availableWidth / pageSize.width();
    double heightZoom = availableHeight / pageSize.height();

    return std::min(widthZoom, heightZoom);
}

double PDFViewHandler::calculateFitWidthZoom(const QSize& viewportSize,
                                             int currentPage,
                                             PageDisplayMode displayMode,
                                             int rotation,
                                             int pageCount,
                                             bool isContinuousScroll,
                                             int verticalScrollBarWidth) const
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return DEFAULT_ZOOM;
    }

    QSizeF pageSize = m_renderer->pageSize(currentPage);
    if (pageSize.isEmpty()) {
        return DEFAULT_ZOOM;
    }

    if (rotation == 90 || rotation == 270) {
        pageSize.transpose();
    }

    if (displayMode == PageDisplayMode::DoublePage) {
        int nextPage = currentPage + 1;
        if (nextPage < pageCount) {
            QSizeF secondPageSize = m_renderer->pageSize(nextPage);
            if (!secondPageSize.isEmpty()) {
                if (rotation == 90 || rotation == 270) {
                    secondPageSize.transpose();
                }
                pageSize.setWidth(pageSize.width() + secondPageSize.width() +
                                  AppConfig::DOUBLE_PAGE_SPACING);
            }
        }
    }

    const int margin = AppConfig::PAGE_MARGIN;
    int availableWidth = viewportSize.width() - 2 * margin;

    if (availableWidth <= 0) {
        return DEFAULT_ZOOM;
    }

    double zoom = availableWidth / pageSize.width();

    // 预留竖直滚动条宽度：若按此缩放排版后内容总高度超过视口高度，竖直滚动条
    // 会出现并挤占可用宽度，导致页面比视口略宽、可以左右移动。这里先按满宽
    // 算出的缩放预判内容高度，若会出现滚动条则扣掉其宽度后重新计算，从而一次
    // 性消除“滚动条出现 → 视口变窄 → 页面偏大”的反馈环。
    if (verticalScrollBarWidth > 0 && viewportSize.height() > 0) {
        int contentHeight = calculateFitWidthContentHeight(
            zoom, rotation, pageCount, currentPage, displayMode, isContinuousScroll);

        if (contentHeight > viewportSize.height()) {
            int adjustedWidth = availableWidth - verticalScrollBarWidth;
            if (adjustedWidth > 0) {
                zoom = adjustedWidth / pageSize.width();
            }
        }
    }

    return zoom;
}

int PDFViewHandler::calculateFitWidthContentHeight(double zoom,
                                                   int rotation,
                                                   int pageCount,
                                                   int currentPage,
                                                   PageDisplayMode displayMode,
                                                   bool isContinuousScroll) const
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return 0;
    }

    const int margin = AppConfig::PAGE_MARGIN;

    auto rotatedPageSize = [this, rotation](int page) {
        QSizeF size = m_renderer->pageSize(page);
        if (rotation == 90 || rotation == 270) {
            size.transpose();
        }
        return size;
    };

    // 连续模式：所有页竖向堆叠，与 calculatePagePositions / sizeHint 的布局一致。
    if (isContinuousScroll) {
        const int pageGap = AppConfig::PAGE_GAP;
        int totalHeight = 0;
        for (int i = 0; i < pageCount; ++i) {
            QSizeF size = rotatedPageSize(i);
            totalHeight += qRound(size.height() * zoom);
            if (i < pageCount - 1) {
                totalHeight += pageGap;
            }
        }
        return totalHeight + 2 * margin;
    }

    // 单页/双页模式：只显示当前一屏，高度取所显示页的最大高度。
    QSizeF size = rotatedPageSize(currentPage);
    double contentHeight = size.height();

    if (displayMode == PageDisplayMode::DoublePage) {
        int nextPage = currentPage + 1;
        if (nextPage < pageCount) {
            QSizeF secondSize = rotatedPageSize(nextPage);
            contentHeight = qMax(contentHeight, secondSize.height());
        }
    }

    return qRound(contentHeight * zoom) + 2 * margin;
}

void PDFViewHandler::requestUpdateZoom(const QSize& viewportSize,
                                       int verticalScrollBarWidth)
{
    const ZoomMode zoomMode = m_state->currentZoomMode();
    const double currentZoom = m_state->currentZoom();

    if (zoomMode == ZoomMode::Custom) {
        return;
    }

    double newZoom = calculateActualZoom(viewportSize, zoomMode, currentZoom,
                                         m_state->currentPage(),
                                         m_state->currentDisplayMode(),
                                         m_state->currentRotation(),
                                         m_state->isContinuousScroll(),
                                         verticalScrollBarWidth);
    newZoom = clampZoom(newZoom);

    if (qAbs(currentZoom - newZoom) > 0.001) {
        emit zoomSettingCompleted(newZoom, zoomMode);
    }
}

void PDFViewHandler::requestSetDisplayMode(PageDisplayMode mode)
{
    int adjustedPage = m_state->currentPage();

    if (mode == PageDisplayMode::DoublePage) {
        adjustedPage = getDoublePageStartIndex(adjustedPage);
    }

    emit displayModeSettingCompleted(mode, adjustedPage);
}

void PDFViewHandler::requestSetContinuousScroll(bool continuous)
{
    emit continuousScrollSettingCompleted(continuous);
}

bool PDFViewHandler::calculatePagePositions(QVector<int>& outPositions,
                                            QVector<int>& outHeights)
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return false;
    }

    const double zoom = m_state->currentZoom();
    const int rotation = m_state->currentRotation();
    const int pageCount = m_state->pageCount();

    outPositions.clear();
    outPositions.reserve(pageCount);
    outHeights.clear();
    outHeights.reserve(pageCount);

    const int pageGap = AppConfig::PAGE_GAP;
    int currentY = 0;

    for (int i = 0; i < pageCount; ++i) {
        QSizeF pageSize = m_renderer->pageSize(i);

        if (rotation == 90 || rotation == 270) {
            pageSize.transpose();
        }

        int height = qRound(pageSize.height() * zoom);

        outPositions.append(currentY);
        outHeights.append(height);

        currentY += height + pageGap;
    }

    emit pagePositionsCalculated(outPositions, outHeights);
    return true;
}

int PDFViewHandler::calculateCurrentPageFromScroll(int scrollY, int margin) const
{
    const QVector<int>& pageYPositions = m_state->pageYPositions();
    if (pageYPositions.isEmpty()) {
        return -1;
    }

    int adjustedY = scrollY - margin;

    for (int i = pageYPositions.size() - 1; i >= 0; --i) {
        if (adjustedY >= pageYPositions[i]) {
            return i;
        }
    }

    return 0;
}

int PDFViewHandler::getScrollPositionForPage(int pageIndex, int margin) const
{
    const QVector<int>& pageYPositions = m_state->pageYPositions();
    if (pageYPositions.isEmpty()) {
        return -1;
    }

    if (pageIndex < 0 || pageIndex >= pageYPositions.size()) {
        return -1;
    }

    return pageYPositions[pageIndex] + margin;
}

QSet<int> PDFViewHandler::getVisiblePages(const QRect& visibleRect,
                                          int preloadMargin,
                                          int margin) const
{
    QSet<int> visiblePages;

    const QVector<int>& pageYPositions = m_state->pageYPositions();
    const QVector<int>& pageHeights = m_state->pageHeights();
    if (pageYPositions.isEmpty()) {
        return visiblePages;
    }

    QRect extended = visibleRect.adjusted(0, -preloadMargin, 0, preloadMargin);

    for (int i = 0; i < pageYPositions.size(); ++i) {
        int pageTop = pageYPositions[i] + margin;
        int pageBottom = pageTop + pageHeights[i];

        if (pageBottom >= extended.top() && pageTop <= extended.bottom()) {
            visiblePages.insert(i);
        }
    }

    return visiblePages;
}

void PDFViewHandler::requestSetRotation(int rotation)
{
    rotation = rotation % 360;
    if (rotation < 0) {
        rotation += 360;
    }

    rotation = (rotation / 90) * 90;

    emit rotationSettingCompleted(rotation);
}


double PDFViewHandler::clampZoom(double zoom) const
{
    return std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
}

bool PDFViewHandler::isValidPageIndex(int pageIndex, int pageCount) const
{
    return pageIndex >= 0 && pageIndex < pageCount;
}
