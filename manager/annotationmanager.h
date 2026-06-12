#ifndef ANNOTATIONMANAGER_H
#define ANNOTATIONMANAGER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QStack>
#include <QVector>

#include "inkstroke.h"

// 批注数据中心：每个 Tab 的 Session 持有一份，按页存储手写笔迹，
// 并维护撤销/重做栈与"未保存"脏标志。与 PageCache/TextCache 平级。
//
// 撤销/重做采用"受影响页前后快照"模型：每个用户操作记录一条 AnnotEdit，
// 内含受影响页（或整文档）操作前后的笔迹快照，undo=回到 before，redo=回到 after。
// 做题场景笔迹量小，快照开销可忽略，换来零歧义的可逆性。
class AnnotationManager : public QObject
{
    Q_OBJECT

public:
    explicit AnnotationManager(QObject* parent = nullptr);

    // —— 用户操作（会进撤销栈、置脏） ——
    void commitStroke(const InkStroke& stroke);
    bool eraseStrokesAt(int pageIndex, const QPointF& pagePos, qreal radius);
    void clearPage(int pageIndex);
    void clearAll();

    void undo();
    void redo();
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    // —— 查询 ——
    const QVector<InkStroke>& strokesForPage(int pageIndex) const;
    int strokeCountForPage(int pageIndex) const;
    QList<int> pagesWithAnnotations() const;   // 升序
    bool isEmpty() const { return m_byPage.isEmpty(); }

    // —— 持久化协作（不进撤销栈、不置脏） ——
    // 打开文档时由 AnnotationPdfIO 灌入已存批注。
    void loadStrokes(const QHash<int, QVector<InkStroke>>& byPage);
    const QHash<int, QVector<InkStroke>>& allStrokes() const { return m_byPage; }
    void reset();                              // 关闭文档时清空全部状态

    bool isDirty() const { return m_dirty; }
    void markSaved();                          // 保存成功后调用，清脏

signals:
    // pageIndex == -1 表示"全部页变化"（清空全部 / 重置 / 加载）。
    void annotationsChanged(int pageIndex);
    void undoStateChanged();
    void dirtyChanged(bool dirty);

private:
    struct AnnotEdit {
        bool wholeDoc = false;                 // true: clearAll，用 *All 快照
        int  pageIndex = -1;                   // 单页操作
        QVector<InkStroke> before, after;
        QHash<int, QVector<InkStroke>> beforeAll, afterAll;
    };

    void pushEdit(const AnnotEdit& edit);
    void applyPageSnapshot(int pageIndex, const QVector<InkStroke>& snapshot);
    void setDirty(bool dirty);

    QHash<int, QVector<InkStroke>> m_byPage;
    QStack<AnnotEdit> m_undo;
    QStack<AnnotEdit> m_redo;
    bool m_dirty = false;

    static const QVector<InkStroke> s_empty;
};

#endif // ANNOTATIONMANAGER_H
