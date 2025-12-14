#ifndef PDFPAGEWIDGET_H
#define PDFPAGEWIDGET_H

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QTimer>

class PDFDocumentSession;
class PerThreadMuPDFRenderer;
class PageCacheManager;
class QScrollArea;

class PDFPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PDFPageWidget(PDFDocumentSession* session, QWidget* parent = nullptr);
    ~PDFPageWidget();

    void setDisplayImages(const QImage& primaryImage, const QImage& secondaryImage = QImage());

    void refreshVisiblePages();

    void setTextSelectionMode(bool enabled);


    void clearHighlights();


    QPointF screenToPageCoord(const QPoint& screenPos, int pageX, int pageY) const;


    int getPageAtPos(const QPoint& pos, int* pageX = nullptr, int* pageY = nullptr) const;


    QScrollArea* getScrollArea() const;


    QSize getViewportSize() const;

    QString getCacheStatistics() const;

    QSize calculateRequiredSize() const;


    void setOCRHoverEnabled(bool enabled);

    void triggerOCRAtCurrentPosition();

signals:
    void pageClicked(int pageIndex, const QPointF& pagePos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);


    void mouseMovedOnPage(int pageIndex, const QPointF& pagePos);

    void mouseLeftAllPages();


    void textSelectionDragging(int pageIndex, const QPointF& pagePos);

    void textSelectionEnded();

    void contextMenuRequested(int pageIndex, const QPointF& pagePos, const QPoint& globalPos);

    void visibleAreaChanged();

    void ocrHoverTriggered(const QImage& image, const QRect& regionRect, const QPoint& lastHoverPos);



protected:

    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    QSize sizeHint() const override;

private:

    void paintSinglePageMode(QPainter& painter);
    void paintDoublePageMode(QPainter& painter);
    void paintContinuousMode(QPainter& painter, const QRect& visibleRect);

    void drawPageImage(QPainter& painter, const QImage& image, int x, int y);
    void drawPagePlaceholder(QPainter& painter, const QRect& rect, int pageIndex);

    void drawOverlays(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom);
    void drawSearchHighlights(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom);
    void drawLinkAreas(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom);
    void drawTextSelection(QPainter& painter, int pageIndex, int pageX, int pageY, double zoom);

private:
    void setupOCRHover();
    QImage extractHoverRegion(const QPoint& pos);
    QRect calculateHoverRect(const QPoint& pos);

    PDFDocumentSession* m_session;
    PerThreadMuPDFRenderer* m_renderer;
    PageCacheManager* m_cacheManager;

    QImage m_currentImage;
    QImage m_secondImage;

    bool m_isTextSelecting;
    QPoint m_dragStartPos;

    bool m_ocrHoverEnabled;
    QPoint m_lastHoverPos;
    QTimer m_hoverTimer;
};

#endif // PDFPAGEWIDGET_H
