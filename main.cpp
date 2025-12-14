#include "mainwindow.h"
#include "stylemanager.h"

#include <QApplication>
#include <QMessageBox>
#include <QTranslator>
#include <QLocale>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

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

    int result = app.exec();

    return result;
}
