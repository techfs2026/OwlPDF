#ifndef OUTLINEMANAGER_H
#define OUTLINEMANAGER_H

#include "outlineitem.h"
#include <QObject>

class PerThreadMuPDFRenderer;

class OutlineManager : public QObject
{
    Q_OBJECT

public:
    explicit OutlineManager(PerThreadMuPDFRenderer* renderer, QObject* parent = nullptr);

    ~OutlineManager();

    bool loadOutline();

    void clear();

    bool hasOutline() const { return m_root && m_root->childCount() > 0; }

    OutlineItem* root() const { return m_root; }

    int totalItemCount() const { return m_totalItems; }

signals:
    void outlineLoaded(bool success, int itemCount);

private:
    int buildOutlineTree(void* fzOutline, OutlineItem* parent);

    int resolvePageIndex(void* fzOutline);

    int countItems(OutlineItem* item) const;

private:
    PerThreadMuPDFRenderer* m_renderer;
    OutlineItem* m_root;
    int m_totalItems;
};

#endif // OUTLINEMANAGER_H
