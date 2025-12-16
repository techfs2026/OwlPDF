#ifndef FILEASSOCIATIONMANAGER_H
#define FILEASSOCIATIONMANAGER_H

#include <QString>
#include <QStringList>

#ifdef Q_OS_WIN

class FileAssociationManager
{
public:
    static FileAssociationManager& instance();

    bool registerFileType(const QString& extension,
                          const QString& fileTypeName,
                          const QString& description,
                          const QString& iconPath = QString());

    bool unregisterFileType(const QString& extension,
                            const QString& fileTypeName);

    bool isRegistered(const QString& extension,
                      const QString& fileTypeName);

    bool registerMultipleTypes(const QStringList& extensions,
                               const QString& fileTypeName,
                               const QString& description);

    bool unregisterMultipleTypes(const QStringList& extensions,
                                 const QString& fileTypeName);

    void refreshIconCache();

    bool isAnyRegistered(const QStringList& extensions,
                         const QString& fileTypeName);

private:
    FileAssociationManager() = default;
    ~FileAssociationManager() = default;
    FileAssociationManager(const FileAssociationManager&) = delete;
    FileAssociationManager& operator=(const FileAssociationManager&) = delete;

    QString getAppPath();
};

#endif

#endif
