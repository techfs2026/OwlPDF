#ifndef INKSTROKE_H
#define INKSTROKE_H

#include <QColor>
#include <QPointF>
#include <QVector>

// 一笔手写批注（钢笔轨迹）。
//
// 坐标基准：未旋转页面坐标、zoom=1，单位即 PDF 点(pt)。
// 与搜索高亮/文本选择一致——存储时与缩放/旋转无关，绘制时再 ×zoom +
// 页面偏移，写回 PDF 时只需 y 翻转 + MediaBox 偏移（详见 AnnotationPdfIO）。
struct InkStroke {
    int              pageIndex = -1;
    QColor           color = Qt::red;
    qreal            width = 2.0;   // 线宽(pt)，绘制时 ×zoom
    QVector<QPointF> points;        // 未旋转页面坐标(pt)

    bool isValid() const { return pageIndex >= 0 && points.size() >= 1; }
};

#endif // INKSTROKE_H
