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

class PDFViewHandler : public QObject
{
    Q_OBJECT

public:
    explicit PDFViewHandler(PerThreadMuPDFRenderer* renderer, QObject* parent = nullptr);
    ~PDFViewHandler();

    void requestGoToPage(int pageIndex,
                         bool adjustForDoublePageMode,
                         PageDisplayMode currentDisplayMode,
                         int currentPage);

    void requestPreviousPage(PageDisplayMode currentDisplayMode,
                             bool isContinuousScroll,
                             int currentPage);

    void requestNextPage(PageDisplayMode currentDisplayMode,
                         bool isContinuousScroll,
                         int currentPage,
                         int pageCount);

    void requestFirstPage(PageDisplayMode currentDisplayMode);

    void requestLastPage(PageDisplayMode currentDisplayMode, int pageCount);

    int getPreviousPageIndex(PageDisplayMode displayMode,
                             bool continuousScroll,
                             int currentPage) const;

    int getNextPageIndex(PageDisplayMode displayMode,
                         bool continuousScroll,
                         int currentPage) const;

    int getDoublePageStartIndex(int pageIndex) const;

    void requestSetZoom(double zoom);

    void requestSetZoomMode(ZoomMode mode);

    void requestZoomIn(double currentZoom);

    void requestZoomOut(double currentZoom);

    double calculateActualZoom(const QSize& viewportSize,
                               ZoomMode zoomMode,
                               double customZoom,
                               int currentPage,
                               PageDisplayMode displayMode,
                               int rotation) const;

    double calculateFitPageZoom(const QSize& viewportSize,
                                int currentPage,
                                int rotation) const;

    double calculateFitWidthZoom(const QSize& viewportSize,
                                 int currentPage,
                                 PageDisplayMode displayMode,
                                 int rotation,
                                 int pageCount) const;

    void requestUpdateZoom(const QSize& viewportSize,
                           ZoomMode zoomMode,
                           double currentZoom,
                           int currentPage,
                           PageDisplayMode displayMode,
                           int rotation);

    void requestSetDisplayMode(PageDisplayMode mode,
                               bool currentContinuousScroll,
                               int currentPage);

    void requestSetContinuousScroll(bool continuous);

    bool calculatePagePositions(double zoom,
                                int rotation,
                                int pageCount,
                                QVector<int>& outPositions,
                                QVector<int>& outHeights);

    int calculateCurrentPageFromScroll(int scrollY,
                                       int margin,
                                       const QVector<int>& pageYPositions) const;

    int getScrollPositionForPage(int pageIndex,
                                 int margin,
                                 const QVector<int>& pageYPositions) const;

    QSet<int> getVisiblePages(const QRect& visibleRect,
                              int preloadMargin,
                              int margin,
                              const QVector<int>& pageYPositions,
                              const QVector<int>& pageHeights) const;

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

    static constexpr double DEFAULT_ZOOM = 1.0;
    static constexpr double MIN_ZOOM = 0.25;
    static constexpr double MAX_ZOOM = 4.0;
    static constexpr double ZOOM_STEP = 0.1;
};

#endif // PDFVIEWHANDLER_H
