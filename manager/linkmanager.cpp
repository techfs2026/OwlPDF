#include "linkmanager.h"
#include "perthreadmupdfrenderer.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QDebug>

LinkManager::LinkManager(PerThreadMuPDFRenderer* renderer, QObject* parent)
    : QObject(parent)
    , m_renderer(renderer)
{
}

LinkManager::~LinkManager()
{
    clear();
}

QVector<PDFLink> LinkManager::loadPageLinks(int pageIndex)
{
    if (m_cachedLinks.contains(pageIndex)) {
        return m_cachedLinks[pageIndex];
    }

    QVector<PDFLink> links;

    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        return links;
    }

    fz_context* ctx = static_cast<fz_context*>(m_renderer->context());
    fz_document* doc = static_cast<fz_document*>(m_renderer->document());

    if (!ctx || !doc) {
        return links;
    }

    fz_page* page = nullptr;

    fz_try(ctx) {
        page = fz_load_page(ctx, doc, pageIndex);

        fz_link* link = fz_load_links(ctx, page);

        for (fz_link* current = link; current; current = current->next) {
            PDFLink pdfLink;

            fz_rect rect = current->rect;
            pdfLink.rect = QRectF(rect.x0, rect.y0,
                                  rect.x1 - rect.x0,
                                  rect.y1 - rect.y0);

            if (current->uri) {
                pdfLink.uri = QString::fromUtf8(current->uri);
            }

            pdfLink.targetPage = resolveLinkTarget(current);

            links.append(pdfLink);
        }

        fz_drop_link(ctx, link);
    }
    fz_always(ctx) {
        if (page) {
            fz_drop_page(ctx, page);
        }
    }
    fz_catch(ctx) {
        qWarning() << "LinkManager: Failed to load links for page" << pageIndex
                   << ":" << fz_caught_message(ctx);
    }

    m_cachedLinks[pageIndex] = links;

    if (!links.isEmpty()) {
        qDebug() << "LinkManager: Found" << links.size() << "links on page" << pageIndex;
    }

    return links;
}

const PDFLink* LinkManager::hitTestLink(int pageIndex, const QPointF& pos, double zoom)
{
    QVector<PDFLink> links = loadPageLinks(pageIndex);

    if (links.isEmpty()) {
        return nullptr;
    }

    QPointF pagePos = pos / zoom;

    for (const PDFLink& link : links) {
        if (link.rect.contains(pagePos)) {
            int index = &link - links.constData();
            return &m_cachedLinks[pageIndex][index];
        }
    }

    return nullptr;
}

void LinkManager::clear()
{
    m_cachedLinks.clear();
}

int LinkManager::resolveLinkTarget(void* fzLink)
{
    if (!fzLink || !m_renderer) {
        return -1;
    }

    fz_link* link = static_cast<fz_link*>(fzLink);
    fz_context* ctx = static_cast<fz_context*>(m_renderer->context());
    fz_document* doc = static_cast<fz_document*>(m_renderer->document());

    if (!ctx || !doc || !link->uri) {
        return -1;
    }

    int pageIndex = -1;

    fz_try(ctx) {
        fz_location loc = fz_resolve_link(ctx, doc, link->uri, nullptr, nullptr);

        pageIndex = fz_page_number_from_location(ctx, doc, loc);
    }
    fz_catch(ctx) {
        pageIndex = -1;
    }

    return pageIndex;
}
