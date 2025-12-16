#include "fileassociationmanager.h"

#ifdef Q_OS_WIN

#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QDebug>

FileAssociationManager& FileAssociationManager::instance()
{
    static FileAssociationManager instance;
    return instance;
}

QString FileAssociationManager::getAppPath()
{
    QString appPath = QCoreApplication::applicationFilePath();
    return QDir::toNativeSeparators(appPath);
}

bool FileAssociationManager::registerFileType(const QString& extension,
                                              const QString& fileTypeName,
                                              const QString& description,
                                              const QString& iconPath)
{
    try {
        QString appPath = getAppPath();

        QSettings regExt(QString("HKEY_CURRENT_USER\\Software\\Classes\\.%1").arg(extension),
                         QSettings::NativeFormat);
        regExt.setValue(".", fileTypeName);

        QSettings regType(QString("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(fileTypeName),
                          QSettings::NativeFormat);
        regType.setValue(".", description);

        QString icon = iconPath.isEmpty() ? QString("\"%1\",0").arg(appPath) : iconPath;
        regType.setValue("DefaultIcon/.", icon);

        regType.setValue("shell/open/command/.", QString("\"%1\" \"%2\"").arg(appPath, "%1"));

        regExt.sync();
        regType.sync();

        qDebug() << "Successfully registered" << extension << "with" << fileTypeName;
        return true;

    } catch (...) {
        qDebug() << "Failed to register file type:" << extension;
        return false;
    }
}

bool FileAssociationManager::unregisterFileType(const QString& extension,
                                                const QString& fileTypeName)
{
    try {
        QSettings regExt(QString("HKEY_CURRENT_USER\\Software\\Classes\\.%1").arg(extension),
                         QSettings::NativeFormat);
        regExt.remove("");

        QSettings regType(QString("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(fileTypeName),
                          QSettings::NativeFormat);
        regType.remove("");

        qDebug() << "Successfully unregistered" << extension;
        return true;

    } catch (...) {
        qDebug() << "Failed to unregister file type:" << extension;
        return false;
    }
}

bool FileAssociationManager::isRegistered(const QString& extension,
                                          const QString& fileTypeName)
{
    QSettings regExt(QString("HKEY_CURRENT_USER\\Software\\Classes\\.%1").arg(extension),
                     QSettings::NativeFormat);
    QString registeredType = regExt.value(".", "").toString();

    return registeredType == fileTypeName;
}

bool FileAssociationManager::registerMultipleTypes(const QStringList& extensions,
                                                   const QString& fileTypeName,
                                                   const QString& description)
{
    bool success = true;
    for (const QString& ext : extensions) {
        if (!registerFileType(ext, fileTypeName, description)) {
            success = false;
        }
    }

    if (success) {
        refreshIconCache();
    }

    return success;
}

bool FileAssociationManager::unregisterMultipleTypes(const QStringList& extensions,
                                                     const QString& fileTypeName)
{
    bool success = true;
    for (const QString& ext : extensions) {
        if (!unregisterFileType(ext, fileTypeName)) {
            success = false;
        }
    }

    if (success) {
        refreshIconCache();
    }

    return success;
}

bool FileAssociationManager::isAnyRegistered(const QStringList& extensions,
                                             const QString& fileTypeName)
{
    for (const QString& ext : extensions) {
        if (isRegistered(ext, fileTypeName)) {
            return true;
        }
    }
    return false;
}

void FileAssociationManager::refreshIconCache()
{
    QProcess::startDetached("ie4uinit.exe", QStringList() << "-show");
}

#endif
