#ifndef OUTLINEITEM_H
#define OUTLINEITEM_H

#include <QString>
#include <QList>
#include <QMetaType>

class OutlineItem
{
public:
    OutlineItem(const QString& title = QString(),
                int pageIndex = -1,
                const QString& uri = QString());

    ~OutlineItem();

    OutlineItem(const OutlineItem&) = delete;
    OutlineItem& operator=(const OutlineItem&) = delete;

    QString title() const { return m_title; }
    void setTitle(const QString& title) { m_title = title; }

    int pageIndex() const { return m_pageIndex; }
    void setPageIndex(int index) { m_pageIndex = index; }

    QString uri() const { return m_uri; }
    void setUri(const QString& uri) { m_uri = uri; }

    bool isExternalLink() const { return !m_uri.isEmpty(); }
    bool isValid() const { return !m_title.isEmpty() || m_pageIndex >= 0; }

    OutlineItem* parent() const { return m_parent; }
    void setParent(OutlineItem* parent) { m_parent = parent; }

    const QList<OutlineItem*>& children() const { return m_children; }

    void addChild(OutlineItem* child);
    bool insertChild(int index, OutlineItem* child);
    bool removeChild(OutlineItem* child);
    OutlineItem* takeChild(int index);
    void clearChildren();

    int childCount() const { return m_children.size(); }
    OutlineItem* child(int index) const;
    int indexOf(OutlineItem* child) const;
    int depth() const;

private:
    QString m_title;
    int m_pageIndex;
    QString m_uri;

    OutlineItem* m_parent;
    QList<OutlineItem*> m_children;
};

Q_DECLARE_METATYPE(OutlineItem*)

#endif // OUTLINEITEM_H
