#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QSettings>
#include <QString>
#include <QSize>
#include <QByteArray>
#include <QVector>
#include <QDir>
#include <QCoreApplication>

// 单个标签页的会话状态（用于「恢复上次会话」）
struct SessionTabState {
    QString filePath;
    int     page             = 0;
    int     zoomMode         = 0;     // ZoomMode 枚举值
    double  zoom             = 1.0;
    int     displayMode      = 0;     // PageDisplayMode 枚举值
    bool    continuousScroll = false;
};

// 整个窗口的会话状态
struct SessionState {
    QVector<SessionTabState> tabs;
    int  activeIndex    = 0;
    bool showNavigation = false;
};

class AppConfig
{
public:
    static AppConfig& instance();

    static constexpr double MIN_ZOOM = 0.25;
    static constexpr double MAX_ZOOM = 5.0;
    static constexpr double ZOOM_STEP = 0.25;
    static constexpr double DEFAULT_ZOOM = 1.0;
    static constexpr int DEFAULT_DPI = 72;

    static constexpr int PAGE_MARGIN = 20;
    static constexpr int PAGE_GAP = 10;
    static constexpr int SHADOW_OFFSET = 3;
    static constexpr int DOUBLE_PAGE_SPACING = 10;

    static constexpr int MAX_TEXT_CACHE_SIZE = -1;
    static constexpr int PDF_TYPE_DETECT_SAMPLE_PAGES = 5;
    static constexpr double TEXT_PDF_THRESHOLD = 0.3;
    static constexpr int TEXT_PRELOAD_PRIORITY_PAGES = 10;

    int maxCacheSize() const { return m_maxCacheSize; }
    void setMaxCacheSize(int size);

    int preloadMargin() const { return m_preloadMargin; }
    void setPreloadMargin(int margin);

    int resizeDebounceDelay() const { return m_resizeDebounceDelay; }
    void setResizeDebounceDelay(int delay);

    QSize defaultWindowSize() const { return m_defaultWindowSize; }

    int maxTextCacheSize() const { return MAX_TEXT_CACHE_SIZE; }
    int pdfTypeDetectSamplePages() const { return PDF_TYPE_DETECT_SAMPLE_PAGES; }
    int textPreloadPriorityPages() const { return TEXT_PRELOAD_PRIORITY_PAGES; }

    bool rememberLastFile() const { return m_rememberLastFile; }
    void setRememberLastFile(bool remember);

    // 会话恢复：保存 / 读取所有标签页
    void saveSession(const QVector<SessionTabState>& tabs,
                     int activeIndex,
                     bool showNavigation);
    SessionState loadSession();

    // 窗口几何（大小 + 位置 + 最大化状态）
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

    QString ocrModelDir() const { return m_ocrModelDir; }
    void setOcrModelDir(const QString& dir) { m_ocrModelDir = dir; }

    int ocrDebounceDelay() const { return m_ocrDebounceDelay; }
    void setOcrDebounceDelay(int delay) { m_ocrDebounceDelay = delay; }

    int ocrHoverRegionSize() const { return m_ocrHoverRegionSize; }
    void setOcrHoverRegionSize(int size) { m_ocrHoverRegionSize = size; }

    QString jiebaDictDir() const { return m_jiebaDictDir; }

    bool debugMode() const { return m_debugMode; }
    void setDebugMode(bool enabled);

    bool hasAskedFileAssociation() const;
    void setHasAskedFileAssociation(bool asked);

    bool fileAssociationUserChoice() const;
    void setFileAssociationUserChoice(bool choice);

    void load();
    void save();
    void resetToDefaults();

private:
    AppConfig();
    ~AppConfig();

    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    void loadDefaults();

private:
    QSettings m_settings;

    int m_maxCacheSize;
    int m_preloadMargin;
    int m_resizeDebounceDelay;

    QSize m_defaultWindowSize;

    bool m_rememberLastFile;
    bool m_debugMode;

    int m_ocrDebounceDelay = 300;
    int m_ocrHoverRegionSize = 200;

    QString m_ocrModelDir = QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        #ifdef Q_OS_MAC
            "/../Resources/ocr/models"
        #else
            "/ocr/models"
        #endif
    );

    QString m_jiebaDictDir = QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        #ifdef Q_OS_MAC
            "/../Resources/ocr/dict"
        #else
            "/ocr/dict"
        #endif
    );
};

#endif
