#include "mainwindow.h"
#include "stylemanager.h"

#include <QApplication>
#include <QMessageBox>
#include <QTranslator>
#include <QLocale>
#include <QTimer>

#ifdef Q_OS_MACOS
#include <QFileOpenEvent>
#include <QStringList>
#include <QUrl>
#include <utility>

// macOS 通过 Apple Event（而非 argv）投递文件打开请求：
// 在「访达」中双击 PDF、或选择「打开方式 → MuQt」时，
// 系统会向 QApplication 发送 QFileOpenEvent。
// 此过滤器捕获该事件并转交主窗口打开；若窗口尚未就绪则先缓存。
class MacFileOpenFilter : public QObject
{
public:
    void setWindow(MainWindow* window)
    {
        m_window = window;
        for (const QString& path : std::as_const(m_pending)) {
            m_window->openFileFromCommandLine(path);
        }
        m_pending.clear();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::FileOpen) {
            auto* openEvent = static_cast<QFileOpenEvent*>(event);
            QString path = openEvent->file();
            if (path.isEmpty() && openEvent->url().isLocalFile()) {
                path = openEvent->url().toLocalFile();
            }
            if (!path.isEmpty()) {
                if (m_window) {
                    m_window->openFileFromCommandLine(path);
                } else {
                    m_pending.append(path);  // 窗口未就绪，先缓存
                }
            }
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    MainWindow* m_window = nullptr;
    QStringList m_pending;
};
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setOrganizationName("MuQt");
    app.setApplicationName("MuQt");
    app.setWindowIcon(QIcon(":/resources/windows.ico"));

#ifdef Q_OS_MACOS
    // 尽早安装，以捕获启动阶段（冷启动打开文件）投递的 QFileOpenEvent
    MacFileOpenFilter macFileOpenFilter;
    app.installEventFilter(&macFileOpenFilter);
#endif

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

#ifdef Q_OS_MACOS
    // 窗口就绪，转交此前缓存的文件打开请求
    macFileOpenFilter.setWindow(&mainWindow);
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
