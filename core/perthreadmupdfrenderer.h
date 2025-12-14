#ifndef PERTHREADMUPDFRENDERER_H
#define PERTHREADMUPDFRENDERER_H

#include <QString>
#include <QImage>
#include <QSizeF>
#include <QVector>
#include <QMutex>
#include "papereffectenhancer.h"
#include "datastructure.h"

extern "C" {
#include <mupdf/fitz.h>
}

enum class RenderScene {
    Page,
    Thumbnail,
    Export,
    Search,
    Print
};


class PerThreadMuPDFRenderer
{
public:
    PerThreadMuPDFRenderer();
    explicit PerThreadMuPDFRenderer(const QString& documentPath);
    ~PerThreadMuPDFRenderer();

    PerThreadMuPDFRenderer(const PerThreadMuPDFRenderer&) = delete;
    PerThreadMuPDFRenderer& operator=(const PerThreadMuPDFRenderer&) = delete;

    bool loadDocument(const QString& filePath, QString* errorMsg = nullptr);

    void closeDocument();

    QString documentPath() const;

    bool isDocumentLoaded() const;

    int pageCount() const;

    QSizeF pageSize(int pageIndex) const;

    RenderResult renderPage(int pageIndex, double zoom, int rotation, RenderScene scene = RenderScene::Page);

    bool extractText(int pageIndex, PageTextData& outData, QString* errorMsg = nullptr);

    bool isTextPDF(int samplePages = 5);

    QString getLastError() const;

    void setPaperEffectEnabled(bool enabled);
    bool paperEffectEnabled() const { return m_paperEffectEnabled; }

    fz_context* context() const { return m_context; }
    fz_document* document() const { return m_document; }


private:
    bool createContext();

    void destroyContext();

    void setLastError(const QString& error) const;

private:
    QString m_documentPath;
    fz_context* m_context;
    fz_document* m_document;
    int m_pageCount;
    mutable QVector<QSizeF> m_pageSizeCache;
    mutable QString m_lastError;

    PaperEffectEnhancer m_paperEffectEnhancer;
    bool m_paperEffectEnabled;
};

#endif // PERTHREADMUPDFRENDERER_H
