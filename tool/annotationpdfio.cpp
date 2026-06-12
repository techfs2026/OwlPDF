#include "annotationpdfio.h"
#include "annotationmanager.h"
#include "inkstroke.h"
#include "perthreadmupdfrenderer.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

#include <QFile>
#include <QHash>
#include <QVector>
#include <QDebug>
#include <vector>

namespace {

// 取某页 MediaBox（用于 y 翻转与原点偏移）。失败时回退到 (0,0,w,h)。
fz_rect pageMediaBox(fz_context* ctx, pdf_document* doc, int pageIndex,
                     PerThreadMuPDFRenderer* renderer)
{
    fz_rect box = { 0, 0, 0, 0 };
    pdf_obj* pageObj = pdf_lookup_page_obj(ctx, doc, pageIndex);
    if (pageObj) {
        pdf_obj* mb = pdf_dict_get_inheritable(ctx, pageObj, PDF_NAME(MediaBox));
        if (mb) {
            box = pdf_to_rect(ctx, mb);
        }
    }
    if (box.x1 <= box.x0 || box.y1 <= box.y0) {
        QSizeF sz = renderer->pageSize(pageIndex);
        box = fz_make_rect(0, 0, sz.width(), sz.height());
    }
    return box;
}

// 删除某页全部 Ink 批注（在内存 doc 上；不落盘）。
void deleteInkAnnots(fz_context* ctx, pdf_document* doc, int pageIndex)
{
    pdf_page* page = pdf_load_page(ctx, doc, pageIndex);
    if (!page) {
        return;
    }
    fz_try(ctx) {
        pdf_annot* annot = pdf_first_annot(ctx, page);
        while (annot) {
            pdf_annot* next = pdf_next_annot(ctx, annot);
            if (pdf_annot_type(ctx, annot) == PDF_ANNOT_INK) {
                pdf_delete_annot(ctx, page, annot);
            }
            annot = next;
        }
    }
    fz_always(ctx) {
        pdf_drop_page(ctx, page);
    }
    fz_catch(ctx) {
        // 删除失败不致命，记录即可
        qWarning() << "AnnotationPdfIO: deleteInkAnnots failed on page" << pageIndex;
    }
}

bool getPdfDoc(PerThreadMuPDFRenderer* renderer, fz_context** ctxOut, pdf_document** docOut)
{
    if (!renderer || !renderer->isDocumentLoaded()) {
        return false;
    }
    fz_context* ctx = static_cast<fz_context*>(renderer->context());
    fz_document* fzdoc = static_cast<fz_document*>(renderer->document());
    if (!ctx || !fzdoc) {
        return false;
    }
    pdf_document* doc = pdf_document_from_fz_document(ctx, fzdoc);
    if (!doc) {
        return false;
    }
    *ctxOut = ctx;
    *docOut = doc;
    return true;
}

}  // namespace

bool AnnotationPdfIO::load(PerThreadMuPDFRenderer* renderer, AnnotationManager* manager)
{
    fz_context* ctx = nullptr;
    pdf_document* doc = nullptr;
    if (!getPdfDoc(renderer, &ctx, &doc) || !manager) {
        return false;
    }

    QHash<int, QVector<InkStroke>> byPage;
    const int pageCount = renderer->pageCount();

    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        pdf_page* page = pdf_load_page(ctx, doc, pageIndex);
        if (!page) {
            continue;
        }

        fz_try(ctx) {
            const fz_rect box = pageMediaBox(ctx, doc, pageIndex, renderer);

            for (pdf_annot* annot = pdf_first_annot(ctx, page); annot;
                 annot = pdf_next_annot(ctx, annot)) {
                if (pdf_annot_type(ctx, annot) != PDF_ANNOT_INK) {
                    continue;
                }

                QColor color = Qt::red;
                int n = 0;
                float c[4] = { 0, 0, 0, 0 };
                pdf_annot_color(ctx, annot, &n, c);
                if (n == 3) {
                    color = QColor::fromRgbF(c[0], c[1], c[2]);
                } else if (n == 1) {
                    color = QColor::fromRgbF(c[0], c[0], c[0]);
                }

                qreal width = pdf_annot_border_width(ctx, annot);
                if (width <= 0) {
                    width = 2.0;
                }

                const int strokeCount = pdf_annot_ink_list_count(ctx, annot);
                for (int s = 0; s < strokeCount; ++s) {
                    InkStroke stroke;
                    stroke.pageIndex = pageIndex;
                    stroke.color = color;
                    stroke.width = width;

                    const int vtx = pdf_annot_ink_list_stroke_count(ctx, annot, s);
                    stroke.points.reserve(vtx);
                    for (int k = 0; k < vtx; ++k) {
                        fz_point p = pdf_annot_ink_list_stroke_vertex(ctx, annot, s, k);
                        // PDF 用户空间 → 未旋转页面坐标(左上原点, y 向下)
                        stroke.points.append(QPointF(p.x - box.x0, box.y1 - p.y));
                    }
                    if (stroke.isValid()) {
                        byPage[pageIndex].append(stroke);
                    }
                }
            }
        }
        fz_always(ctx) {
            pdf_drop_page(ctx, page);
        }
        fz_catch(ctx) {
            qWarning() << "AnnotationPdfIO: failed reading ink on page" << pageIndex;
        }
    }

    manager->loadStrokes(byPage);

    if (byPage.isEmpty()) {
        return false;
    }

    // 读入 overlay 后，从内存 doc 删除 Ink，避免与 overlay 双重绘制（文件不变）。
    for (auto it = byPage.constBegin(); it != byPage.constEnd(); ++it) {
        deleteInkAnnots(ctx, doc, it.key());
    }
    return true;
}

bool AnnotationPdfIO::save(PerThreadMuPDFRenderer* renderer, AnnotationManager* manager,
                           QString* errorMessage)
{
    fz_context* ctx = nullptr;
    pdf_document* doc = nullptr;
    if (!getPdfDoc(renderer, &ctx, &doc) || !manager) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid document");
        return false;
    }

    const QString savePath = renderer->documentPath();
    if (savePath.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("No file path");
        return false;
    }

    // 简易备份：首次保存时复制一份 .owlpdf-backup（已存在则跳过）。
    const QString backupPath = savePath + QStringLiteral(".owlpdf-backup");
    if (!QFile::exists(backupPath)) {
        QFile::copy(savePath, backupPath);
    }

    const QHash<int, QVector<InkStroke>> all = manager->allStrokes();
    bool success = false;

    fz_try(ctx) {
        // 写入：逐页删除旧 Ink，再按 overlay 重建。
        for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
            const int pageIndex = it.key();
            const QVector<InkStroke>& strokes = it.value();
            if (strokes.isEmpty()) {
                continue;
            }

            pdf_page* page = pdf_load_page(ctx, doc, pageIndex);
            if (!page) {
                continue;
            }

            fz_try(ctx) {
                // 清旧 Ink
                pdf_annot* a = pdf_first_annot(ctx, page);
                while (a) {
                    pdf_annot* next = pdf_next_annot(ctx, a);
                    if (pdf_annot_type(ctx, a) == PDF_ANNOT_INK) {
                        pdf_delete_annot(ctx, page, a);
                    }
                    a = next;
                }

                const fz_rect box = pageMediaBox(ctx, doc, pageIndex, renderer);

                for (const InkStroke& stroke : strokes) {
                    if (!stroke.isValid()) {
                        continue;
                    }
                    pdf_annot* annot = pdf_create_annot(ctx, page, PDF_ANNOT_INK);

                    std::vector<fz_point> pts;
                    pts.reserve(stroke.points.size());
                    for (const QPointF& p : stroke.points) {
                        // 未旋转页面坐标 → PDF 用户空间
                        pts.push_back(fz_make_point(static_cast<float>(box.x0 + p.x()),
                                                    static_cast<float>(box.y1 - p.y())));
                    }
                    int count = static_cast<int>(pts.size());
                    pdf_set_annot_ink_list(ctx, annot, 1, &count, pts.data());

                    float col[3] = {
                        static_cast<float>(stroke.color.redF()),
                        static_cast<float>(stroke.color.greenF()),
                        static_cast<float>(stroke.color.blueF())
                    };
                    pdf_set_annot_color(ctx, annot, 3, col);
                    pdf_set_annot_border_width(ctx, annot, static_cast<float>(stroke.width));
                }
            }
            fz_always(ctx) {
                pdf_drop_page(ctx, page);
            }
            fz_catch(ctx) {
                fz_rethrow(ctx);
            }
        }

        pdf_write_options opts = pdf_default_write_options;
        opts.do_incremental = 1;
        opts.do_garbage = 0;

        QByteArray pathBytes = savePath.toUtf8();
        pdf_save_document(ctx, doc, pathBytes.constData(), &opts);
        success = true;
    }
    fz_catch(ctx) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(fz_caught_message(ctx));
        }
        qWarning() << "AnnotationPdfIO: save failed:" << (errorMessage ? *errorMessage : QString());
        return false;
    }

    // 保存后再次从内存 doc 删除 Ink，保持 overlay 为唯一渲染来源。
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            deleteInkAnnots(ctx, doc, it.key());
        }
    }

    manager->markSaved();
    return success;
}
