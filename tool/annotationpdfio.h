#ifndef ANNOTATIONPDFIO_H
#define ANNOTATIONPDFIO_H

#include <QString>

class PerThreadMuPDFRenderer;
class AnnotationManager;

// PDF Ink 批注的读写，照搬 tool/outlineeditor.cpp 的写回模式：
// 取主线程 renderer 的 fz_context / fz_document → pdf_specifics → 增量保存。
//
// 坐标基准：manager 中的笔迹为「未旋转页面坐标(pt)、左上原点、y 向下」。
// PDF 用户空间为左下原点、y 向上，故换算为 pdfX = mediabox.x0 + x，
// pdfY = mediabox.y1 - y（对 /Rotate==0 的页面精确，覆盖绝大多数做题/扫描 PDF）。
//
// 双重绘制规避：load() 读入 overlay 后，把 Ink 批注从内存 doc 删除，
// 使页面渲染不再画出 Ink，统一交给 overlay。文件本身不变，直到显式 save()。
namespace AnnotationPdfIO {

// 打开文档后调用：把已存 Ink 批注读入 manager，并从内存 doc 删除它们。
// 返回是否读到任何批注（用于决定是否清缓存重渲）。
bool load(PerThreadMuPDFRenderer* renderer, AnnotationManager* manager);

// 保存：把 manager 当前全部笔迹写回 PDF（先删页面所有 Ink，再按 overlay 重建），
// 增量保存到原文件；保存后再次从内存 doc 删除 Ink 以保持 overlay 为唯一来源。
// 成功时 manager->markSaved()。errorMessage 选填。
bool save(PerThreadMuPDFRenderer* renderer, AnnotationManager* manager,
          QString* errorMessage = nullptr);

}  // namespace AnnotationPdfIO

#endif // ANNOTATIONPDFIO_H
