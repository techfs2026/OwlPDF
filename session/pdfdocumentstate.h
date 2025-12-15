#ifndef PDFDOCUMENTSTATE_H
#define PDFDOCUMENTSTATE_H

#include <QObject>
#include <QString>
#include <QVector>
#include "datastructure.h"

class PDFDocumentState : public QObject
{
    Q_OBJECT

public:
    explicit PDFDocumentState(QObject* parent = nullptr);
    ~PDFDocumentState();

    bool isDocumentLoaded() const { return m_isDocumentLoaded; }
    QString documentPath() const { return m_documentPath; }
    int pageCount() const { return m_pageCount; }
    bool isTextPDF() const { return m_isTextPDF; }
    int currentPage() const { return m_currentPage; }
    double currentZoom() const { return m_currentZoom; }
    ZoomMode currentZoomMode() const { return m_currentZoomMode; }
    PageDisplayMode currentDisplayMode() const { return m_currentDisplayMode; }
    bool isContinuousScroll() const { return m_isContinuousScroll; }
    int currentRotation() const { return m_currentRotation; }
    const QVector<int>& pageYPositions() const { return m_pageYPositions; }
    const QVector<int>& pageHeights() const { return m_pageHeights; }
    bool linksVisible() const { return m_linksVisible; }
    bool hasTextSelection() const { return m_hasTextSelection; }
    bool isSearching() const { return m_isSearching; }
    int searchTotalMatches() const { return m_searchTotalMatches; }
    int searchCurrentMatchIndex() const { return m_searchCurrentMatchIndex; }

    void setDocumentLoaded(bool loaded, const QString& path = QString(),
                           int pageCount = 0, bool isTextPDF = false);
    void setCurrentPage(int pageIndex);
    void setCurrentZoom(double zoom);
    void setCurrentZoomMode(ZoomMode mode);
    void setCurrentDisplayMode(PageDisplayMode mode);
    void setContinuousScroll(bool continuous);
    void setCurrentRotation(int rotation);
    void setPagePositions(const QVector<int>& positions, const QVector<int>& heights);
    void setLinksVisible(bool visible);
    void setHasTextSelection(bool has);
    void setSearchState(bool searching, int totalMatches = 0, int currentIndex = -1);

    void reset();
    void saveViewportState(int scrollY);
    int getRestoredScrollPosition(int margin) const;
    bool needRestoreViewport() const { return m_viewportRestore.needRestore; }
    void clearViewportRestore() { m_viewportRestore.reset(); }

private:
    bool m_isDocumentLoaded;
    QString m_documentPath;
    int m_pageCount;
    bool m_isTextPDF;

    int m_currentPage;

    double m_currentZoom;
    ZoomMode m_currentZoomMode;

    PageDisplayMode m_currentDisplayMode;
    bool m_isContinuousScroll;
    int m_currentRotation;
    ViewportRestoreState m_viewportRestore;

    QVector<int> m_pageYPositions;
    QVector<int> m_pageHeights;

    bool m_linksVisible;
    bool m_hasTextSelection;
    bool m_isSearching;
    int m_searchTotalMatches;
    int m_searchCurrentMatchIndex;
};

#endif // PDFDOCUMENTSTATE_H
