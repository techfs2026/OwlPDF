#ifndef PDFANNOTATIONHANDLER_H
#define PDFANNOTATIONHANDLER_H

#include <QObject>
#include <QColor>
#include <QPointF>

#include "inkstroke.h"

class AnnotationManager;

// 当前交互工具。Pen/Eraser 由本 handler 处理；None 时回到浏览/文本选择。
enum class AnnotTool { None, Pen, Eraser };

// 批注交互编排：持有当前工具与笔/橡皮配置，承接页面控件已归一化的
// 「未旋转页面坐标(pt)」，组装 InkStroke 落库给 AnnotationManager。
// 坐标的屏幕↔页面↔旋转换算在 PDFPageWidget 完成，本 handler 不碰屏幕坐标。
class PDFAnnotationHandler : public QObject
{
    Q_OBJECT

public:
    explicit PDFAnnotationHandler(AnnotationManager* manager, QObject* parent = nullptr);

    AnnotTool tool() const { return m_tool; }
    void setTool(AnnotTool tool);

    QColor penColor() const { return m_penColor; }
    void setPenColor(const QColor& color) { m_penColor = color; }

    qreal penWidth() const { return m_penWidth; }
    void setPenWidth(qreal width) { m_penWidth = width; }

    qreal eraserRadius() const { return m_eraserRadius; }
    void setEraserRadius(qreal radius) { m_eraserRadius = radius; }

    // 一笔的生命周期；pt 为未旋转页面坐标(pt)。
    void beginStroke(int pageIndex, const QPointF& pt);
    void extendStroke(const QPointF& pt);
    void endStroke();
    void cancelStroke();
    bool isDrawing() const { return m_drawing; }
    const InkStroke& currentStroke() const { return m_current; }

    void eraseAt(int pageIndex, const QPointF& pt);

    AnnotationManager* manager() const { return m_manager; }

signals:
    void toolChanged(AnnotTool tool);

private:
    AnnotationManager* m_manager;

    AnnotTool m_tool = AnnotTool::None;
    QColor m_penColor = QColor(220, 30, 30);
    qreal m_penWidth = 2.0;
    qreal m_eraserRadius = 10.0;

    InkStroke m_current;
    bool m_drawing = false;
};

#endif // PDFANNOTATIONHANDLER_H
