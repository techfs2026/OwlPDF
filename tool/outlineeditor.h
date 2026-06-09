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
    OutlineItem* root() const { return m_root; }
    void setRoot(OutlineItem* root);

    // 启动时清理备份目录：删除超期备份，并在总体积超限时从最旧删起。
    // 备份统一落在 AppDataLocation/backups（见 createBackup）。
    static void cleanupBackups();

signals:
    void outlineModified();
    void saveCompleted(bool success, const QString& errorMsg);
    // 仅在脏标记真正翻转时发出，供上层维护"未保存"提示
    void unsavedChangesChanged(bool hasUnsaved);

private:
    // 脏标记唯一写入口：仅值变化时通知
    void setModified(bool modified);

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
