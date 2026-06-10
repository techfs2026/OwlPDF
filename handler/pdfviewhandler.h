#ifndef PDFVIEWHANDLER_H
#define PDFVIEWHANDLER_H

#include <QObject>
#include <QSize>
#include <QSizeF>
#include <QVector>
#include <QSet>
#include <QRect>

#include "datastructure.h"

class PerThreadMuPDFRenderer;
class PDFDocumentState;

class PDFViewHandler : public QObject
{
    Q_OBJECT

public:
    explicit PDFViewHandler(PerThreadMuPDFRenderer* renderer,
                            const PDFDocumentState* state,
                            QObject* parent = nullptr);
    ~PDFViewHandler();

    void requestGoToPage(int pageIndex, bool adjustForDoublePageMode = true);

    void requestPreviousPage();

    void requestNextPage();

    void requestFirstPage();

    void requestLastPage();

    int getPreviousPageIndex(PageDisplayMode displayMode,
                             bool continuousScroll,
                             int currentPage) const;

    int getNextPageIndex(PageDisplayMode displayMode,
                         bool continuousScroll,
                         int currentPage) const;

    int getDoublePageStartIndex(int pageIndex) const;

    void requestSetZoom(double zoom);

    void requestSetZoomMode(ZoomMode mode);

    void requestZoomIn();

    void requestZoomOut();

    double calculateActualZoom(const QSize& viewportSize,
                               ZoomMode zoomMode,
                               double customZoom,
                               int currentPage,
                               PageDisplayMode displayMode,
                               int rotation,
                               bool isContinuousScroll = false,
                               int verticalScrollBarWidth = 0) const;

    double calculateFitPageZoom(const QSize& viewportSize,
                                int currentPage,
                                int rotation) const;

    double calculateFitWidthZoom(const QSize& viewportSize,
                                 int currentPage,
                                 PageDisplayMode displayMode,
                                 int rotation,
                                 int pageCount,
                                 bool isContinuousScroll = false,
                                 int verticalScrollBarWidth = 0) const;

    void requestUpdateZoom(const QSize& viewportSize,
                           int verticalScrollBarWidth = 0);

    void requestSetDisplayMode(PageDisplayMode mode);

    void requestSetContinuousScroll(bool continuous);

    bool calculatePagePositions(QVector<int>& outPositions,
                                QVector<int>& outHeights);

    // 计算给定缩放下当前显示内容的总高度（用于判断竖直滚动条是否会出现）
    int calculateFitWidthContentHeight(double zoom,
                                       int rotation,
                                       int pageCount,
                                       int currentPage,
                                       PageDisplayMode displayMode,
                                       bool isContinuousScroll) const;

    int calculateCurrentPageFromScroll(int scrollY, int margin) const;

    int getScrollPositionForPage(int pageIndex, int margin) const;

    QSet<int> getVisiblePages(const QRect& visibleRect,
                              int preloadMargin,
                              int margin) const;

    void requestSetRotation(int rotation);

    double clampZoom(double zoom) const;

    bool isValidPageIndex(int pageIndex, int pageCount) const;

signals:

    void pageNavigationCompleted(int newPageIndex);

    void zoomSettingCompleted(double newZoom, ZoomMode newMode);

    void displayModeSettingCompleted(PageDisplayMode newMode, int adjustedPage);

    void continuousScrollSettingCompleted(bool continuous);

    void rotationSettingCompleted(int newRotation);

    void pagePositionsCalculated(const QVector<int>& positions,
                                 const QVector<int>& heights);

    void scrollToPositionRequested(int scrollY);

    void currentPageUpdatedFromScroll(int newPageIndex);

private:
    PerThreadMuPDFRenderer* m_renderer;
    const PDFDocumentState* m_state;

    static constexpr double DEFAULT_ZOOM = 1.0;
    static constexpr double MIN_ZOOM = 0.25;
    static constexpr double MAX_ZOOM = 4.0;
    static constexpr double ZOOM_STEP = 0.1;
};

#endif // PDFVIEWHANDLER_H
