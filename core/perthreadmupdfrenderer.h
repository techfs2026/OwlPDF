#ifndef PERTHREADMUPDFRENDERER_H
#define PERTHREADMUPDFRENDERER_H

#include <QString>
#include <QImage>
#include <QSizeF>
#include <QVector>
#include <QMutex>
#include "scanenhancer.h"
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

    RenderResult renderPage(int pageIndex, double zoom, int rotation,
                            RenderScene scene = RenderScene::Page,
                            double devicePixelRatio = 1.0);

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

    // 扫描 PDF 原始字节判断是否含 JPEG2000(JPX) 图像（滤镜名 "JPXDecode"）。
    // 仅含 JPX 的文档才需要串行化 fz_run_page，普通文档可全速并行（详见 .cpp 顶部说明）。
    static bool documentUsesJpx(const QString& filePath);

private:
    QString m_documentPath;
    fz_context* m_context;
    fz_document* m_document;
    int m_pageCount;
    mutable QVector<QSizeF> m_pageSizeCache;
    mutable QString m_lastError;

    ScanEnhancer m_scanEnhancer;
    bool m_paperEffectEnabled;

    // 文档级标志：该文档是否含 JPX，决定 fz_run_page 是否需要进程级串行化。
    // 打开文档时检测一次，之后只读、不再变化。
    bool m_serializePageRun = false;
};

#endif // PERTHREADMUPDFRENDERER_H
