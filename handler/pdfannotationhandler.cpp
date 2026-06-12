#include "pdfannotationhandler.h"
#include "annotationmanager.h"

#include <QLineF>

PDFAnnotationHandler::PDFAnnotationHandler(AnnotationManager* manager, QObject* parent)
    : QObject(parent)
    , m_manager(manager)
{
}

void PDFAnnotationHandler::setTool(AnnotTool tool)
{
    if (m_tool == tool) {
        return;
    }
    cancelStroke();
    m_tool = tool;
    emit toolChanged(tool);
}

void PDFAnnotationHandler::beginStroke(int pageIndex, const QPointF& pt)
{
    m_current = InkStroke{};
    m_current.pageIndex = pageIndex;
    m_current.color = m_penColor;
    m_current.width = m_penWidth;
    m_current.points.append(pt);
    m_drawing = true;
}

void PDFAnnotationHandler::extendStroke(const QPointF& pt)
{
    if (!m_drawing) {
        return;
    }
    // 丢弃与上一点几乎重合的采样，避免点数膨胀。
    if (!m_current.points.isEmpty() &&
        QLineF(m_current.points.last(), pt).length() < 0.75) {
        return;
    }
    m_current.points.append(pt);
}

void PDFAnnotationHandler::endStroke()
{
    if (!m_drawing) {
        return;
    }
    m_drawing = false;
    if (m_manager && m_current.isValid()) {
        m_manager->commitStroke(m_current);
    }
    m_current = InkStroke{};
}

void PDFAnnotationHandler::cancelStroke()
{
    m_drawing = false;
    m_current = InkStroke{};
}

void PDFAnnotationHandler::eraseAt(int pageIndex, const QPointF& pt)
{
    if (m_manager) {
        m_manager->eraseStrokesAt(pageIndex, pt, m_eraserRadius);
    }
}
