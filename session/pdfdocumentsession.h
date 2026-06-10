#ifndef PDFDOCUMENTSESSION_H
#define PDFDOCUMENTSESSION_H

#include <QObject>
#include <QString>
#include <QSize>
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

    void goToPage(int pageIndex, bool adjustForDoublePageMode = true);
    void previousPage();
    void nextPage();
    void firstPage();
    void lastPage();

    void zoomIn();
    void zoomOut();
    void updateZoom(const QSize& viewportSize, int verticalScrollBarWidth = 0);

    void setDisplayMode(PageDisplayMode mode);

    void calculatePagePositions();
    void updateCurrentPageFromScroll(int scrollY, int margin = 0);
    int getScrollPositionForPage(int pageIndex, int margin = 0) const;

    void saveViewportState(int scrollY);
    void clearViewportRestore();

    void setPaperEffectEnabled(bool enabled);

signals:
    void documentLoaded(const QString& path, int pageCount);
    void currentPageChanged(int pageIndex);
    void zoomSettingCompleted(double zoom, ZoomMode mode);
    void currentZoomChanged(double zoom);
    void requestCurrentScrollPosition();
    void currentDisplayModeChanged(PageDisplayMode mode);
    void continuousScrollChanged(bool continuous);
    void currentRotationChanged(int rotation);
    void pagePositionsChanged(const QVector<int>& positions, const QVector<int>& heights);
    void scrollToPositionRequested(int scrollY);
    void textSelectionChanged(bool hasSelection);
    void searchCompleted(const QString& query, int totalMatches);
    void searchCancelled();
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
