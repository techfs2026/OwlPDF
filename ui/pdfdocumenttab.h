#ifndef PDFDOCUMENTTAB_H
#define PDFDOCUMENTTAB_H

#include <QWidget>
#include <QDateTime>

#include "datastructure.h"
#include "iocrengine.h"
#include "ocrmanager.h"

class PDFDocumentSession;
class PDFPageWidget;
class SearchWidget;
class QScrollArea;
class QProgressBar;
class OCRFloatingWidget;

class PDFDocumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit PDFDocumentTab(QWidget* parent = nullptr);
    ~PDFDocumentTab();

    bool loadDocument(const QString& filePath, QString* errorMessage = nullptr);
    void closeDocument();
    bool isDocumentLoaded() const;
    QString documentPath() const;
    QString documentTitle() const;


    void previousPage();
    void nextPage();
    void firstPage();
    void lastPage();
    void goToPage(int pageIndex);

    void zoomIn();
    void zoomOut();
    void actualSize();
    void fitPage();
    void fitWidth();
    void setZoom(double zoom);

    void setDisplayMode(PageDisplayMode mode);
    void setContinuousScroll(bool continuous);

    void showSearchBar();
    void hideSearchBar();
    bool isSearchBarVisible() const;


    void copySelectedText();
    void selectAll();


    void setLinksVisible(bool visible);
    bool linksVisible() const;


    int currentPage() const;
    int pageCount() const;
    double zoom() const;
    ZoomMode zoomMode() const;
    PageDisplayMode displayMode() const;
    bool isContinuousScroll() const;
    bool hasTextSelection() const;
    bool isTextPDF() const;


    // 文档会话：供 MainWindow 把公共侧边栏挂载到当前文档
    PDFDocumentSession* session() const { return m_session; }

    // 目录是否有未保存修改；保存目录到 PDF
    bool hasUnsavedChanges() const;
    bool saveOutline();


    SearchWidget* searchWidget() const { return m_searchWidget; }


    QSize getViewportSize() const;


    void updateZoom(const QSize& viewportSize);

    void findNext();


    void findPrevious();

    void setPaperEffectEnabled(bool enabled);
    bool paperEffectEnabled() const;


    bool isOCRHoverEnabled() const { return OCRManager::instance().isOCRHoverEnabled(); }
    void updateOCRHoverState();
    void triggerOCRAtCurrentPosition();

signals:

    void documentLoaded(const QString& filePath, int pageCount);
    void documentError(const QString& errorMessage);
    void pageChanged(int pageIndex);
    void zoomChanged(double zoom);
    void displayModeChanged(PageDisplayMode mode);
    void continuousScrollChanged(bool continuous);
    void searchCompleted(const QString& query, int totalMatches);
    void textSelectionChanged();
    void paperEffectChanged(bool enabled);
    void unsavedChangesChanged(bool hasUnsaved);

private slots:

    void onDocumentLoaded(const QString& filePath, int pageCount);
    void onPageChanged(int pageIndex);
    void onZoomChanged(double zoom);
    void onDisplayModeChanged(PageDisplayMode mode);
    void onContinuousScrollChanged(bool continuous);
    void onPagePositionsChanged(const QVector<int>& positions, const QVector<int>& heights);
    void onTextSelectionChanged(bool hasSelection);
    void onTextPreloadProgress(int current, int total);
    void onTextPreloadCompleted();
    void onSearchCompleted(const QString& query, int totalMatches);

    void onPageClicked(int pageIndex, const QPointF& pagePos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void onMouseMovedOnPage(int pageIndex, const QPointF& pagePos);
    void onMouseLeftAllPages();
    void onTextSelectionDragging(int pageIndex, const QPointF& pagePos);
    void onTextSelectionEnded();
    void onContextMenuRequested(int pageIndex, const QPointF& pagePos, const QPoint& globalPos);
    void onVisibleAreaChanged();

    void onScrollValueChanged(int value);

    void onOCRHoverTriggered(const QImage& image, const QRect& regionRect, const QPoint& lastHoverPos);
    void onOCRCompleted(const QVector<TokenWithPosition>& tokens, const QRect& regionRect, const QPoint& lastHoverPos);
    void onOCRFailed(const QString& error);
    void onLookupRequested(const QString& text);

private:

    void setupUI();
    void setupConnections();

    void renderAndUpdatePages();

    QImage renderPage(int pageIndex);

    void refreshVisiblePages();

    void updateScrollBarPolicy();
    // 适合宽度计算时需要预留的竖直滚动条宽度（按需出现时会挤占视口宽度）
    int verticalScrollBarReserve() const;
    void updateCursorForPage(int pageIndex, const QPointF& pagePos);
    void showContextMenu(int pageIndex, const QPointF& pagePos, const QPoint& globalPos);

    // 搜索导航后把命中词滚动到视野内合适的位置
    void scrollToSearchResult(const SearchResult& result);

private:
    PDFDocumentSession* m_session;
    PDFPageWidget* m_pageWidget;
    SearchWidget* m_searchWidget;

    QScrollArea* m_scrollArea;
    QProgressBar* m_textPreloadProgress;

    qint64 m_lastClickTime;
    QPoint m_lastClickPos;
    int m_clickCount;

    bool m_isUserScrolling;

    // 搜索定位滚动期间为 true，用于抑制 onPagePositionsChanged 把滚动拉回页顶
    bool m_inSearchScroll = false;

    OCRFloatingWidget* m_ocrFloatingWidget;
    QImage m_lastOCRImage;
    QRect m_lastOCRRegion;
    QPoint m_lastHoverPos;
};

#endif // PDFDOCUMENTTAB_H
