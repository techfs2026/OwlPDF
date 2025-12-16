#include "mainwindow.h"
#include "stylemanager.h"

#include <QApplication>
#include <QMessageBox>
#include <QTranslator>
#include <QLocale>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setOrganizationName("MuQt");
    app.setApplicationName("MuQt");
    app.setWindowIcon(QIcon(":/resources/windows.ico"));

    StyleManager::instance().initialize();
    StyleManager::instance().setTheme("light");
    StyleManager::instance().applyStyleToApplication(&app);

    QTranslator translator;
    QString locale = QLocale::system().name();

    if (translator.load(":/translations/muqt_" + locale)) {
        app.installTranslator(&translator);
    }

    MainWindow mainWindow;
    mainWindow.show();

#ifdef Q_OS_WIN
    QTimer::singleShot(500, &mainWindow, [&mainWindow]() {
        mainWindow.checkAndShowFirstRunDialog();
    });
#endif

    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        QTimer::singleShot(100, &mainWindow, [&mainWindow, filePath]() {
            mainWindow.openFileFromCommandLine(filePath);
        });
    }

    int result = app.exec();

    return result;
}
