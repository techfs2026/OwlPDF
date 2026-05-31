#include "perthreadmupdfrenderer.h"
#include <QDebug>
#include <QThread>
#include <QMutex>
#include <QFile>
#include <cstring>

namespace {
    // 每个 PerThreadMuPDFRenderer 拥有独立的 fz_context / fz_document，常规对象互不共享，
    // 但 MuPDF 仍有进程级共享状态。最危险的是 JPEG2000(JPX) 解码：它走 OpenJPEG，
    // 而 MuPDF 的锁只覆盖 FZ_LOCK_ALLOC/FREETYPE/GLYPHCACHE（无 JPX 锁），
    // OpenJPEG 回调用于分配内存的 fz_context 是进程级共享且未加锁的。
    // 多个工作线程（TextCacheManager 的批量提取任务）同时解码含 JPX 图像的页面时会竞争，
    // 某线程读到 NULL context，在 fz_calloc_no_throw 处触发 EXC_BAD_ACCESS 崩溃。
    // 用进程级互斥量串行化 fz_run_page 即可规避该竞争。
    // 为避免拖慢不含 JPX 的普通文档，这把锁只在 m_serializePageRun==true（打开时检测到 JPX）
    // 的文档上启用；普通文档完全不加锁，保持原有的多线程并行。
    QMutex g_mupdfPageRunMutex;
}


PerThreadMuPDFRenderer::PerThreadMuPDFRenderer()
    : m_context(nullptr)
    , m_document(nullptr)
    , m_pageCount(0)
    , m_paperEffectEnabled(false)
{
}

PerThreadMuPDFRenderer::PerThreadMuPDFRenderer(const QString& documentPath)
    : m_documentPath(documentPath)
    , m_context(nullptr)
    , m_document(nullptr)
    , m_pageCount(0)
    , m_paperEffectEnabled(false)
{
    if (!createContext()) {
        qCritical() << "PerThreadMuPDFRenderer: Failed to initialize context";
        return;
    }

    QByteArray pathUtf8 = m_documentPath.toUtf8();

    // 仅含 JPX 的文档才需要串行化 fz_run_page；普通文档保持并行，不受性能影响。
    m_serializePageRun = documentUsesJpx(m_documentPath);

    fz_try(m_context) {
        m_document = fz_open_document(m_context, pathUtf8.constData());
        m_pageCount = fz_count_pages(m_context, m_document);

        m_pageSizeCache.resize(m_pageCount);
        for (int i = 0; i < m_pageCount; ++i) {
            m_pageSizeCache[i] = QSizeF();
        }

        qInfo() << "PerThreadMuPDFRenderer: Successfully initialized with"
                << m_pageCount << "pages"
                << "JPX serialize:" << m_serializePageRun
                << "Thread:" << QThread::currentThreadId();
    }
    fz_catch(m_context) {
        QString err = QString("Failed to open document: %1")
        .arg(fz_caught_message(m_context));
        setLastError(err);
        qCritical() << "PerThreadMuPDFRenderer:" << err;

        m_document = nullptr;
        m_pageCount = 0;
        destroyContext();
    }
}

PerThreadMuPDFRenderer::~PerThreadMuPDFRenderer()
{
    if (isDocumentLoaded()) {
        if (m_document && m_context) {
            fz_drop_document(m_context, m_document);
            m_document = nullptr;
        }

        m_pageCount = 0;
        m_pageSizeCache.clear();
        m_documentPath.clear();
    }

    if (m_context) {
        fz_drop_context(m_context);
        m_context = nullptr;
    }

    qInfo() << "PerThreadMuPDFRenderer: Destroyed"
            << "Thread:" << QThread::currentThreadId();
}

bool PerThreadMuPDFRenderer::createContext()
{
    if (m_context) {
        qWarning() << "PerThreadMuPDFRenderer: Context already exists";
        return true;
    }

    m_context = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);

    if (!m_context) {
        setLastError("Failed to create MuPDF context");
        qCritical() << "PerThreadMuPDFRenderer: Failed to create context";
        return false;
    }

    fz_try(m_context) {
        fz_register_document_handlers(m_context);
    }
    fz_catch(m_context) {
        QString err = QString("Failed to register document handlers: %1")
        .arg(fz_caught_message(m_context));
        setLastError(err);
        qCritical() << "PerThreadMuPDFRenderer:" << err;

        fz_drop_context(m_context);
        m_context = nullptr;
        return false;
    }

    return true;
}

void PerThreadMuPDFRenderer::destroyContext()
{
    if (!m_context) {
        return;
    }

    fz_drop_context(m_context);
    m_context = nullptr;
}

bool PerThreadMuPDFRenderer::documentUsesJpx(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        // 打不开就当作不含 JPX：真正的打开失败会在 fz_open_document 处报错。
        return false;
    }

    static const QByteArray needle = QByteArrayLiteral("JPXDecode");
    const int overlap = needle.size() - 1;     // 跨块边界需要保留的尾部字节数
    const qint64 chunkSize = 1 << 20;          // 每次读 1MB，避免一次性载入大文件

    QByteArray window;
    while (!f.atEnd()) {
        const QByteArray chunk = f.read(chunkSize);
        if (chunk.isEmpty())
            break;
        window.append(chunk);
        if (window.contains(needle))
            return true;
        // 只保留尾部 overlap 字节，既防内存膨胀，又不漏掉跨两块边界的匹配。
        if (window.size() > overlap)
            window = window.right(overlap);
    }
    return false;
}

bool PerThreadMuPDFRenderer::loadDocument(const QString& filePath, QString* errorMsg)
{
    if (isDocumentLoaded()) {
        if (m_document && m_context) {
            fz_drop_document(m_context, m_document);
            m_document = nullptr;
        }
        m_pageCount = 0;
        m_pageSizeCache.clear();
        m_documentPath.clear();

        destroyContext();
    }

    if (!createContext()) {
        QString err = "Failed to create context";
        setLastError(err);
        if (errorMsg) *errorMsg = err;
        return false;
    }

    QByteArray pathUtf8 = filePath.toUtf8();

    // 仅含 JPX 的文档才需要串行化 fz_run_page；普通文档保持并行，不受性能影响。
    m_serializePageRun = documentUsesJpx(filePath);

    fz_try(m_context) {
        m_document = fz_open_document(m_context, pathUtf8.constData());
        m_pageCount = fz_count_pages(m_context, m_document);

        m_pageSizeCache.resize(m_pageCount);
        for (int i = 0; i < m_pageCount; ++i) {
            m_pageSizeCache[i] = QSizeF();
        }

        m_documentPath = filePath;

        qInfo() << "PerThreadMuPDFRenderer: Document loaded successfully -"
                << m_pageCount << "pages"
                << "JPX serialize:" << m_serializePageRun;
    }
    fz_catch(m_context) {
        QString err = QString("Failed to open document: %1")
        .arg(fz_caught_message(m_context));
        setLastError(err);
        if (errorMsg) *errorMsg = err;

        qCritical() << "PerThreadMuPDFRenderer:" << err;

        m_document = nullptr;
        m_pageCount = 0;
        m_pageSizeCache.clear();
        m_documentPath.clear();

        destroyContext();

        return false;
    }

    return true;
}

void PerThreadMuPDFRenderer::closeDocument()
{
    if (!m_document && !m_context) {
        return;
    }

    qInfo() << "PerThreadMuPDFRenderer: Closing document";

    if (m_document && m_context) {
        qDebug() << "PerThreadMuPDFRenderer: Dropping document";
        fz_drop_document(m_context, m_document);
        m_document = nullptr;
    }

    m_pageCount = 0;
    m_pageSizeCache.clear();
    m_documentPath.clear();

    destroyContext();

    qInfo() << "PerThreadMuPDFRenderer: Document closed";
}

QString PerThreadMuPDFRenderer::documentPath() const
{
    return m_documentPath;
}

bool PerThreadMuPDFRenderer::isDocumentLoaded() const
{
    return m_document != nullptr && m_context != nullptr;
}

int PerThreadMuPDFRenderer::pageCount() const
{
    return m_pageCount;
}

QSizeF PerThreadMuPDFRenderer::pageSize(int pageIndex) const
{
    if (!isDocumentLoaded() || pageIndex < 0 || pageIndex >= m_pageCount) {
        return QSizeF();
    }

    if (!m_pageSizeCache[pageIndex].isEmpty()) {
        return m_pageSizeCache[pageIndex];
    }

    QSizeF size;
    fz_page* page = nullptr;

    fz_try(m_context) {
        page = fz_load_page(m_context, m_document, pageIndex);
        fz_rect bounds = fz_bound_page(m_context, page);
        size.setWidth(bounds.x1 - bounds.x0);
        size.setHeight(bounds.y1 - bounds.y0);

        m_pageSizeCache[pageIndex] = size;
    }
    fz_always(m_context) {
        if (page) fz_drop_page(m_context, page);
    }
    fz_catch(m_context) {
        QString err = QString("Failed to get page size for page %1: %2")
        .arg(pageIndex)
            .arg(fz_caught_message(m_context));
        setLastError(err);
        qWarning() << "PerThreadMuPDFRenderer:" << err;
        size = QSizeF();   // 失败时返回空尺寸，不缓存
    }

    return size;
}

static fz_matrix calculateMatrixForMuPDF(double zoom, int rotation)
{
    fz_matrix matrix = fz_scale(zoom, zoom);
    int normalized = rotation % 360;
    if (normalized < 0) normalized += 360;
    if (normalized != 0) {
        matrix = fz_concat(matrix, fz_rotate(normalized));
    }
    return matrix;
}

static QImage pixmapToQImage(fz_context* ctx, fz_pixmap* pixmap)
{
    if (!pixmap) return QImage();

    int width = fz_pixmap_width(ctx, pixmap);
    int height = fz_pixmap_height(ctx, pixmap);
    int stride = fz_pixmap_stride(ctx, pixmap);
    unsigned char* samples = fz_pixmap_samples(ctx, pixmap);

    // 注意：这里必须 deep copy。pixmap 在本函数返回后会被 drop，
    // 而 QImage(samples, ...) 的浅包装会指向已释放内存。
    QImage image(width, height, QImage::Format_RGB888);
    for (int y = 0; y < height; ++y) {
        unsigned char* src = samples + y * stride;
        unsigned char* dest = image.scanLine(y);
        memcpy(dest, src, width * 3);
    }

    return image;
}

RenderResult PerThreadMuPDFRenderer::renderPage(int pageIndex, double zoom, int rotation,
                                                RenderScene scene, double devicePixelRatio)
{
    RenderResult result;

    if (!isDocumentLoaded()) {
        result.errorMessage = "No document loaded";
        return result;
    }

    if (pageIndex < 0 || pageIndex >= m_pageCount) {
        result.errorMessage = QString("Invalid page index %1").arg(pageIndex);
        return result;
    }

    double actualZoom = zoom;
    bool applyPaperEffect = m_paperEffectEnabled;

    switch (scene) {
    case RenderScene::Thumbnail:
        actualZoom = qMin(zoom, 2.0);
        applyPaperEffect = false;
        break;

    case RenderScene::Page:
        actualZoom = zoom;
        applyPaperEffect = m_paperEffectEnabled;
        break;

    case RenderScene::Export:
    case RenderScene::Print:
        actualZoom = qMax(zoom, 2.0);
        applyPaperEffect = false;
        break;

    case RenderScene::Search:
        actualZoom = qMin(zoom, 1.5);
        applyPaperEffect = false;
        break;
    }

    // HiDPI 适配：按物理像素渲染。Retina 屏 dpr=2 时位图分辨率翻倍，
    // 配合下方 setDevicePixelRatio，避免 QPainter 把图二次放大导致文字发虚。
    if (devicePixelRatio > 1.0) {
        actualZoom *= devicePixelRatio;
    }

    // 指针提到 fz_try 外声明并置空，fz_always 才能访问到它们。
    fz_page*   page   = nullptr;
    fz_device* device = nullptr;
    fz_pixmap* pixmap = nullptr;

    fz_try(m_context) {
        page = fz_load_page(m_context, m_document, pageIndex);

        fz_matrix matrix = calculateMatrixForMuPDF(actualZoom, rotation);
        fz_rect bounds = fz_bound_page(m_context, page);
        bounds = fz_transform_rect(bounds, matrix);

        pixmap = fz_new_pixmap_with_bbox(
            m_context,
            fz_device_rgb(m_context),
            fz_round_rect(bounds),
            nullptr,
            0
            );
        fz_clear_pixmap_with_value(m_context, pixmap, 0xff);

        device = fz_new_draw_device(m_context, fz_identity, pixmap);
        // 仅当文档含 JPX 时才串行化页面运行，规避 OpenJPEG/JPX 的进程级竞争（详见文件顶部说明）；
        // 普通文档 m_serializePageRun=false，保持并行、无锁开销。
        // 注意：fz_run_page 通过 setjmp/longjmp 抛错，longjmp 会跳过 C++ 析构，因此不能用
        // QMutexLocker，必须用 fz_always 保证解锁恰好一次。m_serializePageRun 打开后固定不变，
        // lock/unlock 读到的值一致，配对始终正确。
        if (m_serializePageRun) g_mupdfPageRunMutex.lock();
        fz_try(m_context) {
            fz_run_page(m_context, page, device, matrix, nullptr);
        }
        fz_always(m_context) {
            if (m_serializePageRun) g_mupdfPageRunMutex.unlock();
        }
        fz_catch(m_context) {
            fz_rethrow(m_context);
        }

        // close 可能抛异常（flush 操作），留在 try 内；drop 移到 always。
        fz_close_device(m_context, device);

        result.image = pixmapToQImage(m_context, pixmap);

        if (applyPaperEffect && !result.image.isNull()) {
            result.image = m_scanEnhancer.enhance(result.image);
        }

        // 标记 dpr：位图含物理像素，绘制时 Qt 才会按逻辑尺寸正确显示。
        // enhance() 会生成新 QImage，故必须放在它之后。
        if (!result.image.isNull() && devicePixelRatio > 0.0) {
            result.image.setDevicePixelRatio(devicePixelRatio);
        }

        result.success = true;
    }
    fz_always(m_context) {
        // 无论成功还是异常都会执行；fz_drop_* 对 nullptr 安全。
        if (device) fz_drop_device(m_context, device);
        if (pixmap) fz_drop_pixmap(m_context, pixmap);
        if (page)   fz_drop_page(m_context, page);
    }
    fz_catch(m_context) {
        QString err = QString("Failed to render page %1: %2")
        .arg(pageIndex)
            .arg(fz_caught_message(m_context));
        setLastError(err);
        result.errorMessage = err;
        result.success = false;
        result.image = QImage();   // 异常时确保不返回半成品图像
        qWarning() << "PerThreadMuPDFRenderer:" << err;
    }

    return result;
}

void PerThreadMuPDFRenderer::setPaperEffectEnabled(bool enabled)
{
    m_paperEffectEnabled = enabled;
}

bool PerThreadMuPDFRenderer::extractText(int pageIndex, PageTextData& outData, QString* errorMsg)
{
    if (!isDocumentLoaded()) {
        if (errorMsg) *errorMsg = "Document not loaded";
        return false;
    }

    if (pageIndex < 0 || pageIndex >= m_pageCount) {
        if (errorMsg) *errorMsg = QString("Invalid page index %1").arg(pageIndex);
        return false;
    }

    outData = PageTextData();
    outData.pageIndex = pageIndex;

    fz_stext_page* stext  = nullptr;
    fz_page*       page   = nullptr;
    fz_device*     dev    = nullptr;
    bool           ok     = false;

    fz_try(m_context) {
        page = fz_load_page(m_context, m_document, pageIndex);

        fz_rect bound = fz_bound_page(m_context, page);
        stext = fz_new_stext_page(m_context, bound);

        fz_stext_options opts;
        memset(&opts, 0, sizeof(opts));
        opts.flags = 0;

        dev = fz_new_stext_device(m_context, stext, &opts);
        // 文本提取不需要图像像素：跳过图像解码，既能加速，也能尽量避开 JPX 解码路径
        fz_enable_device_hints(m_context, dev, FZ_DONT_DECODE_IMAGES);
        // 仅当文档含 JPX 时才串行化（普通文档保持并行、无锁开销）。详见上方 renderPage 内的说明。
        if (m_serializePageRun) g_mupdfPageRunMutex.lock();
        fz_try(m_context) {
            fz_run_page(m_context, page, dev, fz_identity, nullptr);
        }
        fz_always(m_context) {
            if (m_serializePageRun) g_mupdfPageRunMutex.unlock();
        }
        fz_catch(m_context) {
            fz_rethrow(m_context);
        }
        fz_close_device(m_context, dev);   // close 留在 try

        for (fz_stext_block* block = stext->first_block; block; block = block->next) {
            if (block->type != FZ_STEXT_BLOCK_TEXT) continue;

            TextBlock tb;
            tb.bbox = QRectF(block->bbox.x0, block->bbox.y0,
                             block->bbox.x1 - block->bbox.x0,
                             block->bbox.y1 - block->bbox.y0);

            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                TextLine tl;
                tl.bbox = QRectF(line->bbox.x0, line->bbox.y0,
                                 line->bbox.x1 - line->bbox.x0,
                                 line->bbox.y1 - line->bbox.y0);

                for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                    TextChar tc;
                    tc.character = QChar(ch->c);

                    fz_quad q = ch->quad;
                    qreal minX = qMin(qMin(q.ul.x, q.ur.x), qMin(q.ll.x, q.lr.x));
                    qreal maxX = qMax(qMax(q.ul.x, q.ur.x), qMax(q.ll.x, q.lr.x));
                    qreal minY = qMin(qMin(q.ul.y, q.ur.y), qMin(q.ll.y, q.lr.y));
                    qreal maxY = qMax(qMax(q.ul.y, q.ur.y), qMax(q.ll.y, q.lr.y));

                    tc.bbox = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));

                    tl.chars.append(tc);
                    outData.fullText.append(tc.character);
                }

                tb.lines.append(tl);
                outData.fullText.append('\n');
            }

            outData.blocks.append(tb);
            outData.fullText.append("\n\n");
        }

        ok = true;
    }
    fz_always(m_context) {
        // dev 在 close 后仍需 drop；stext / page 统一在此释放。
        if (dev)   fz_drop_device(m_context, dev);
        if (stext) fz_drop_stext_page(m_context, stext);
        if (page)  fz_drop_page(m_context, page);
    }
    fz_catch(m_context) {
        QString err = QString("Failed to extract text on page %1: %2")
        .arg(pageIndex)
            .arg(fz_caught_message(m_context));
        setLastError(err);
        if (errorMsg) *errorMsg = err;
        qWarning() << "PerThreadMuPDFRenderer:" << err;
        ok = false;
    }

    return ok;
}

bool PerThreadMuPDFRenderer::isTextPDF(int samplePages)
{
    if (!isDocumentLoaded() || m_pageCount == 0) {
        return false;
    }

    int pagesToCheck = samplePages;
    if (pagesToCheck <= 0 || pagesToCheck > m_pageCount) {
        pagesToCheck = m_pageCount;
    }

    int textPageCount = 0;

    for (int i = 0; i < pagesToCheck; ++i) {
        bool hasText = false;

        fz_page*       page   = nullptr;
        fz_stext_page* stext  = nullptr;
        fz_device*     device = nullptr;

        fz_try(m_context) {
            page = fz_load_page(m_context, m_document, i);
            stext = fz_new_stext_page(m_context, fz_bound_page(m_context, page));

            fz_stext_options options;
            memset(&options, 0, sizeof(options));

            device = fz_new_stext_device(m_context, stext, &options);
            // 仅当文档含 JPX 时才串行化（普通文档保持并行、无锁开销）。详见上方 renderPage 内的说明。
            if (m_serializePageRun) g_mupdfPageRunMutex.lock();
            fz_try(m_context) {
                fz_run_page(m_context, page, device, fz_identity, nullptr);
            }
            fz_always(m_context) {
                if (m_serializePageRun) g_mupdfPageRunMutex.unlock();
            }
            fz_catch(m_context) {
                fz_rethrow(m_context);
            }
            fz_close_device(m_context, device);

            for (fz_stext_block* block = stext->first_block; block; block = block->next) {
                if (block->type == FZ_STEXT_BLOCK_TEXT) {
                    for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                        for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                            if (ch->c > 32) {
                                hasText = true;
                                break;
                            }
                        }
                        if (hasText) break;
                    }
                    if (hasText) break;
                }
            }
        }
        fz_always(m_context) {
            if (device) fz_drop_device(m_context, device);
            if (stext)  fz_drop_stext_page(m_context, stext);
            if (page)   fz_drop_page(m_context, page);
        }
        fz_catch(m_context) {
            hasText = false;
            qWarning() << "PerThreadMuPDFRenderer: isTextPDF check failed on page" << i
                       << ":" << fz_caught_message(m_context);
        }

        if (hasText) {
            textPageCount++;
        }
    }

    double ratio = static_cast<double>(textPageCount) / pagesToCheck;
    return ratio >= 0.3;
}

QString PerThreadMuPDFRenderer::getLastError() const
{
    return m_lastError;
}

void PerThreadMuPDFRenderer::setLastError(const QString& error) const
{
    m_lastError = error;
}