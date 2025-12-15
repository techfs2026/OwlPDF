#ifndef OUTLINEEDITOR_H
#define OUTLINEEDITOR_H

#include <QObject>
#include <QString>

class PerThreadMuPDFRenderer;
class OutlineItem;

class OutlineEditor : public QObject
{
    Q_OBJECT

public:
    explicit OutlineEditor(PerThreadMuPDFRenderer* renderer, QObject* parent = nullptr);
    ~OutlineEditor();

    OutlineItem* addOutline(OutlineItem* parentItem,
                            const QString& title,
                            int pageIndex,
                            int insertIndex = -1);

    bool deleteOutline(OutlineItem* item);
    bool deleteAllOutlines();
    bool renameOutline(OutlineItem* item, const QString& newTitle);
    bool updatePageIndex(OutlineItem* item, int newPageIndex);
    bool moveOutline(OutlineItem* item, OutlineItem* newParent, int newIndex = -1);
    bool saveToDocument(const QString& filePath = QString());

    bool hasUnsavedChanges() const { return m_modified; }
    void resetModifiedFlag() { m_modified = false; }
    OutlineItem* root() const { return m_root; }
    void setRoot(OutlineItem* root);

signals:
    void outlineModified();
    void saveCompleted(bool success, const QString& errorMsg);

private:
    void* createPdfOutline(void* ctx, void* doc, OutlineItem* item);
    void* buildPdfOutlineTree(void* ctx, void* doc, OutlineItem* item, void* pdfParent);
    bool validateOutline(const QString& title, int pageIndex) const;
    int findItemIndex(OutlineItem* item) const;
    bool removeFromParent(OutlineItem* item);
    QString createBackup(const QString& filePath) const;

private:
    PerThreadMuPDFRenderer* m_renderer;
    OutlineItem* m_root;
    bool m_modified;
};

#endif // OUTLINEEDITOR_H
