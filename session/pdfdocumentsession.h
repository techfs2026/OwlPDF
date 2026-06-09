#ifndef PDFDOCUMENTSESSION_H
#define PDFDOCUMENTSESSION_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QSize>
#include <QPointF>
#include <memory>

#include "datastructure.h"
#include "textcachemanager.h"
#include "pdfcontenthandler.h"
#include "pdfdocumentstate.h"

class PerThreadMuPDFRenderer;
class PageCacheManager;
class PDFViewHandler;
class PDFContentHandler;
class PDFInteractionHandler;
class PDFDocumentState;
class OutlineItem;
class OutlineEditor;
struct SearchResult;
struct PDFLink;

class PDFDocumentSession : public QObject
{
    Q_OBJECT

public:
    explicit PDFDocumentSession(QObject* parent = nullptr);
    ~PDFDocumentSession();

    PerThreadMuPDFRenderer* renderer() const { return m_renderer.get(); }
    PageCacheManager* pageCache() const { return m_pageCache.get(); }
    TextCacheManager* textCache() const { return m_textCache.get(); }

    PDFViewHandler* viewHandler() const { return m_viewHandler.get(); }
    PDFContentHandler* contentHandler() const { return m_contentHandler.get(); }
    PDFInteractionHandler* interactionHandler() const { return m_interactionHandler.get(); }

    const PDFDocumentState* state() const { return m_state.get(); }

    bool loadDocument(const QString& filePath, QString* errorMessage = nullptr);
    void closeDocument();
    bool isDocumentLoaded() const;
    QString documentPath() const;
    int pageCount() const;
    bool isTextPDF(int samplePages = 5) const;

    void goToPage(int pageIndex, bool adjustForDoublePageMode = true);
    void previousPage();
    void nextPage();
    void firstPage();
    void lastPage();

    void setZoom(double zoom);
    void setZoomMode(ZoomMode mode);
    void zoomIn();
    void zoomOut();
    void actualSize();
    void fitPage();
    void fitWidth();
    void updateZoom(const QSize& viewportSize, int verticalScrollBarWidth = 0);

    void setDisplayMode(PageDisplayMode mode);
    void setContinuousScroll(bool continuous);
    void setRotation(int rotation);

    bool loadOutline();
    OutlineItem* outlineRoot() const;
    OutlineEditor* outlineEditor() const;
    bool hasUnsavedOutlineChanges() const;
    bool saveOutlineChanges();

    void loadThumbnails();
    QImage getThumbnail(int pageIndex, bool preferHighRes = false) const;
    bool hasThumbnail(int pageIndex) const;
    QString getThumbnailStatistics() const;

    void startSearch(const QString& query,
                     bool caseSensitive = false,
                     bool wholeWords = false,
                     int startPage = 0);
    void cancelSearch();
    void clearSearch();
    SearchResult findNext();
    SearchResult findPrevious();
    SearchResult findFirstFromStartPage();

    void startTextSelection(int pageIndex, const QPointF& pagePos, double zoom);
    void updateTextSelection(int pageIndex, const QPointF& pagePos, double zoom);
    void extendTextSelection(int pageIndex, const QPointF& pagePos, double zoom);
    void endTextSelection();
    void clearTextSelection();
    void selectWord(int pageIndex, const QPointF& pagePos, double zoom);
    void selectLine(int pageIndex, const QPointF& pagePos, double zoom);
    void selectAll(int pageIndex);
    void copySelectedText();

    void setLinksVisible(bool visible);
    const PDFLink* hitTestLink(int pageIndex, const QPointF& pagePos, double zoom);
    void clearHoveredLink();
    bool handleLinkClick(const PDFLink* link);

    void calculatePagePositions();
    void updateCurrentPageFromScroll(int scrollY, int margin = 0);
    int getScrollPositionForPage(int pageIndex, int margin = 0) const;

    QString getCacheStatistics() const;
    QString getTextCacheStatistics() const;

    void saveViewportState(int scrollY);
    void clearViewportRestore();

    void setPaperEffectEnabled(bool enabled);
    bool paperEffectEnabled() const;

signals:
    void documentLoaded(const QString& path, int pageCount);
    void documentTypeChanged(bool isTextPDF);
    void documentError(const QString& error);
    void currentPageChanged(int pageIndex);
    void zoomSettingCompleted(double zoom, ZoomMode mode);
    void currentZoomChanged(double zoom);
    void requestCurrentScrollPosition();
    void currentZoomModeChanged(ZoomMode mode);
    void currentDisplayModeChanged(PageDisplayMode mode);
    void continuousScrollChanged(bool continuous);
    void currentRotationChanged(int rotation);
    void pagePositionsChanged(const QVector<int>& positions, const QVector<int>& heights);
    void scrollToPositionRequested(int scrollY);
    void linksVisibleChanged(bool visible);
    void textSelectionChanged(bool hasSelection);
    void searchStateChanged(bool searching, int totalMatches, int currentIndex);
    void outlineLoaded(bool success, int itemCount);
    void unsavedOutlineChangesChanged(bool hasUnsaved);
    void thumbnailLoaded(int pageIndex, const QImage& thumbnail);
    void thumbnailLoadProgress(int current, int total);
    void searchProgressUpdated(int currentPage, int totalPages, int matchCount);
    void searchCompleted(const QString& query, int totalMatches);
    void searchCancelled();
    void linkHovered(const PDFLink* link);
    void internalLinkRequested(int targetPage);
    void externalLinkRequested(const QString& uri);
    void textCopied(int characterCount);
    void textPreloadProgress(int loaded, int total);
    void textPreloadCompleted();
    void textPreloadCancelled();
    void paperEffectChanged(bool enabled);

private:
    void setupConnections();
    void updateCacheAfterStateChange();

private:
    std::unique_ptr<PerThreadMuPDFRenderer> m_renderer;
    std::unique_ptr<PageCacheManager> m_pageCache;
    std::unique_ptr<TextCacheManager> m_textCache;

    std::unique_ptr<PDFViewHandler> m_viewHandler;
    std::unique_ptr<PDFContentHandler> m_contentHandler;
    std::unique_ptr<PDFInteractionHandler> m_interactionHandler;

    std::unique_ptr<PDFDocumentState> m_state;
};

#endif // PDFDOCUMENTSESSION_H
