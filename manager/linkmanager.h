#ifndef LINKMANAGER_H
#define LINKMANAGER_H

#include <QObject>
#include <QRectF>
#include <QVector>
#include <QString>
#include <QMap>

class PerThreadMuPDFRenderer;

struct PDFLink
{
    QRectF rect;
    int targetPage;
    QString uri;

    bool isInternal() const { return targetPage >= 0; }

    bool isExternal() const { return !uri.isEmpty() && targetPage < 0; }
};

class LinkManager : public QObject
{
    Q_OBJECT

public:
    explicit LinkManager(PerThreadMuPDFRenderer* renderer, QObject* parent = nullptr);

    ~LinkManager();

    QVector<PDFLink> loadPageLinks(int pageIndex);

    const PDFLink* hitTestLink(int pageIndex, const QPointF& pos, double zoom);

    void clear();

signals:
    void pageJumpRequested(int pageIndex);

    void externalLinkRequested(const QString& uri);

private:
    int resolveLinkTarget(void* fzLink);

private:
    PerThreadMuPDFRenderer* m_renderer;
    QMap<int, QVector<PDFLink>> m_cachedLinks;
};

#endif // LINKMANAGER_H
