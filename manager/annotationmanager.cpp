#include "annotationmanager.h"

#include <QLineF>
#include <algorithm>

const QVector<InkStroke> AnnotationManager::s_empty;

AnnotationManager::AnnotationManager(QObject* parent)
    : QObject(parent)
{
}

namespace {
// 点到线段的最短距离（页面坐标系）。
qreal distancePointToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const qreal dx = b.x() - a.x();
    const qreal dy = b.y() - a.y();
    const qreal len2 = dx * dx + dy * dy;
    if (len2 <= 1e-9) {
        return QLineF(p, a).length();   // 退化为点
    }
    qreal t = ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / len2;
    t = qBound(0.0, t, 1.0);
    const QPointF proj(a.x() + t * dx, a.y() + t * dy);
    return QLineF(p, proj).length();
}

// 橡皮（圆心 pos，半径 radius）是否命中整条笔迹。
bool strokeHit(const InkStroke& stroke, const QPointF& pos, qreal radius)
{
    const qreal threshold = radius + stroke.width / 2.0;
    const QVector<QPointF>& pts = stroke.points;
    if (pts.size() == 1) {
        return QLineF(pos, pts.first()).length() <= threshold;
    }
    for (int i = 1; i < pts.size(); ++i) {
        if (distancePointToSegment(pos, pts[i - 1], pts[i]) <= threshold) {
            return true;
        }
    }
    return false;
}
}

const QVector<InkStroke>& AnnotationManager::strokesForPage(int pageIndex) const
{
    auto it = m_byPage.constFind(pageIndex);
    return it == m_byPage.constEnd() ? s_empty : it.value();
}

int AnnotationManager::strokeCountForPage(int pageIndex) const
{
    return strokesForPage(pageIndex).size();
}

QList<int> AnnotationManager::pagesWithAnnotations() const
{
    QList<int> pages;
    for (auto it = m_byPage.constBegin(); it != m_byPage.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            pages.append(it.key());
        }
    }
    std::sort(pages.begin(), pages.end());
    return pages;
}

void AnnotationManager::commitStroke(const InkStroke& stroke)
{
    if (!stroke.isValid()) {
        return;
    }

    AnnotEdit edit;
    edit.pageIndex = stroke.pageIndex;
    edit.before = strokesForPage(stroke.pageIndex);
    edit.after = edit.before;
    edit.after.append(stroke);

    m_byPage[stroke.pageIndex] = edit.after;
    pushEdit(edit);
    emit annotationsChanged(stroke.pageIndex);
}

bool AnnotationManager::eraseStrokesAt(int pageIndex, const QPointF& pagePos, qreal radius)
{
    auto it = m_byPage.find(pageIndex);
    if (it == m_byPage.end() || it.value().isEmpty()) {
        return false;
    }

    const QVector<InkStroke>& before = it.value();
    QVector<InkStroke> after;
    after.reserve(before.size());
    for (const InkStroke& s : before) {
        if (!strokeHit(s, pagePos, radius)) {
            after.append(s);
        }
    }

    if (after.size() == before.size()) {
        return false;   // 未命中任何笔
    }

    AnnotEdit edit;
    edit.pageIndex = pageIndex;
    edit.before = before;
    edit.after = after;

    applyPageSnapshot(pageIndex, after);
    pushEdit(edit);
    emit annotationsChanged(pageIndex);
    return true;
}

void AnnotationManager::clearPage(int pageIndex)
{
    auto it = m_byPage.find(pageIndex);
    if (it == m_byPage.end() || it.value().isEmpty()) {
        return;
    }

    AnnotEdit edit;
    edit.pageIndex = pageIndex;
    edit.before = it.value();
    // after 为空

    applyPageSnapshot(pageIndex, QVector<InkStroke>());
    pushEdit(edit);
    emit annotationsChanged(pageIndex);
}

void AnnotationManager::clearAll()
{
    if (m_byPage.isEmpty()) {
        return;
    }

    AnnotEdit edit;
    edit.wholeDoc = true;
    edit.beforeAll = m_byPage;
    // afterAll 为空

    m_byPage.clear();
    pushEdit(edit);
    emit annotationsChanged(-1);
}

void AnnotationManager::undo()
{
    if (m_undo.isEmpty()) {
        return;
    }

    const AnnotEdit edit = m_undo.pop();
    m_redo.push(edit);

    if (edit.wholeDoc) {
        m_byPage = edit.beforeAll;
        emit annotationsChanged(-1);
    } else {
        applyPageSnapshot(edit.pageIndex, edit.before);
        emit annotationsChanged(edit.pageIndex);
    }

    setDirty(true);
    emit undoStateChanged();
}

void AnnotationManager::redo()
{
    if (m_redo.isEmpty()) {
        return;
    }

    const AnnotEdit edit = m_redo.pop();
    m_undo.push(edit);

    if (edit.wholeDoc) {
        m_byPage = edit.afterAll;
        emit annotationsChanged(-1);
    } else {
        applyPageSnapshot(edit.pageIndex, edit.after);
        emit annotationsChanged(edit.pageIndex);
    }

    setDirty(true);
    emit undoStateChanged();
}

void AnnotationManager::loadStrokes(const QHash<int, QVector<InkStroke>>& byPage)
{
    m_byPage = byPage;
    // 去掉空页，保持 pagesWithAnnotations 干净
    for (auto it = m_byPage.begin(); it != m_byPage.end(); ) {
        if (it.value().isEmpty()) {
            it = m_byPage.erase(it);
        } else {
            ++it;
        }
    }
    m_undo.clear();
    m_redo.clear();
    setDirty(false);
    emit undoStateChanged();
    emit annotationsChanged(-1);
}

void AnnotationManager::reset()
{
    const bool had = !m_byPage.isEmpty();
    m_byPage.clear();
    m_undo.clear();
    m_redo.clear();
    setDirty(false);
    emit undoStateChanged();
    if (had) {
        emit annotationsChanged(-1);
    }
}

void AnnotationManager::markSaved()
{
    setDirty(false);
}

void AnnotationManager::pushEdit(const AnnotEdit& edit)
{
    m_undo.push(edit);
    m_redo.clear();
    setDirty(true);
    emit undoStateChanged();
}

void AnnotationManager::applyPageSnapshot(int pageIndex, const QVector<InkStroke>& snapshot)
{
    if (snapshot.isEmpty()) {
        m_byPage.remove(pageIndex);
    } else {
        m_byPage[pageIndex] = snapshot;
    }
}

void AnnotationManager::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        return;
    }
    m_dirty = dirty;
    emit dirtyChanged(dirty);
}
