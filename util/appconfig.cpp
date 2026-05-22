#include "appconfig.h"
#include <QApplication>

AppConfig::AppConfig()
    : m_settings(QSettings::IniFormat, QSettings::UserScope,
                 QApplication::organizationName(),
                 QApplication::applicationName())
{
    loadDefaults();
    load();
}

AppConfig::~AppConfig()
{
    save();
}

AppConfig& AppConfig::instance()
{
    static AppConfig instance;
    return instance;
}

void AppConfig::loadDefaults()
{
    m_maxCacheSize = 10;
    m_preloadMargin = 500;
    m_resizeDebounceDelay = 150;
    m_defaultWindowSize = QSize(1300, 800);
    m_rememberLastFile = true;
    m_debugMode = false;
    m_ocrDebounceDelay = 300;
    m_ocrHoverRegionSize = 200;
}

void AppConfig::load()
{
    m_maxCacheSize = m_settings.value("Cache/MaxSize", m_maxCacheSize).toInt();
    m_preloadMargin = m_settings.value("Cache/PreloadMargin", m_preloadMargin).toInt();
    m_resizeDebounceDelay = m_settings.value("Performance/ResizeDebounceDelay",
                                             m_resizeDebounceDelay).toInt();
    m_rememberLastFile = m_settings.value("Preferences/RememberLastFile",
                                          m_rememberLastFile).toBool();
    m_ocrDebounceDelay = m_settings.value("OCR/DebounceDelay", m_ocrDebounceDelay).toInt();
    m_ocrHoverRegionSize = m_settings.value("OCR/HoverRegionSize", m_ocrHoverRegionSize).toInt();
    m_debugMode = m_settings.value("Debug/Enabled", m_debugMode).toBool();
}

void AppConfig::save()
{
    m_settings.setValue("Cache/MaxSize", m_maxCacheSize);
    m_settings.setValue("Cache/PreloadMargin", m_preloadMargin);
    m_settings.setValue("Performance/ResizeDebounceDelay", m_resizeDebounceDelay);
    m_settings.setValue("Preferences/RememberLastFile", m_rememberLastFile);
    m_settings.setValue("OCR/DebounceDelay", m_ocrDebounceDelay);
    m_settings.setValue("OCR/HoverRegionSize", m_ocrHoverRegionSize);
    m_settings.setValue("Debug/Enabled", m_debugMode);
    m_settings.sync();
}

void AppConfig::resetToDefaults()
{
    // 只重置「偏好」相关分组；保留 LastSession / FileAssociation / Window
    m_settings.remove("Cache");
    m_settings.remove("Performance");
    m_settings.remove("Preferences");
    m_settings.remove("OCR");
    m_settings.remove("Debug");

    loadDefaults();
    save();
}

void AppConfig::setMaxCacheSize(int size)
{
    if (size > 0 && size <= 100) {
        m_maxCacheSize = size;
    }
}

void AppConfig::setPreloadMargin(int margin)
{
    if (margin >= 0 && margin <= 2000) {
        m_preloadMargin = margin;
    }
}

void AppConfig::setResizeDebounceDelay(int delay)
{
    if (delay >= 0 && delay <= 1000) {
        m_resizeDebounceDelay = delay;
    }
}

void AppConfig::setRememberLastFile(bool remember)
{
    m_rememberLastFile = remember;
}

void AppConfig::setDebugMode(bool enabled)
{
    m_debugMode = enabled;
}

void AppConfig::saveSession(const QVector<SessionTabState>& tabs,
                            int activeIndex,
                            bool showNavigation)
{
    // 整组清掉再写，避免上次更长的会话残留条目
    m_settings.remove("LastSession");

    m_settings.beginWriteArray("LastSession/Tabs");
    for (int i = 0; i < tabs.size(); ++i) {
        m_settings.setArrayIndex(i);
        const SessionTabState& t = tabs.at(i);
        m_settings.setValue("FilePath", t.filePath);
        m_settings.setValue("Page", t.page);
        m_settings.setValue("ZoomMode", t.zoomMode);
        m_settings.setValue("Zoom", t.zoom);
        m_settings.setValue("DisplayMode", t.displayMode);
        m_settings.setValue("ContinuousScroll", t.continuousScroll);
    }
    m_settings.endArray();

    m_settings.setValue("LastSession/ActiveIndex", activeIndex);
    m_settings.setValue("LastSession/ShowNavigation", showNavigation);
    m_settings.sync();
}

SessionState AppConfig::loadSession()
{
    SessionState session;

    int count = m_settings.beginReadArray("LastSession/Tabs");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        SessionTabState t;
        t.filePath         = m_settings.value("FilePath").toString();
        t.page             = m_settings.value("Page", 0).toInt();
        t.zoomMode         = m_settings.value("ZoomMode", 0).toInt();
        t.zoom             = m_settings.value("Zoom", 1.0).toDouble();
        t.displayMode      = m_settings.value("DisplayMode", 0).toInt();
        t.continuousScroll = m_settings.value("ContinuousScroll", false).toBool();
        if (!t.filePath.isEmpty()) {
            session.tabs.append(t);
        }
    }
    m_settings.endArray();

    session.activeIndex    = m_settings.value("LastSession/ActiveIndex", 0).toInt();
    session.showNavigation = m_settings.value("LastSession/ShowNavigation", false).toBool();
    return session;
}

QByteArray AppConfig::windowGeometry() const
{
    return m_settings.value("Window/Geometry").toByteArray();
}

void AppConfig::setWindowGeometry(const QByteArray& geometry)
{
    m_settings.setValue("Window/Geometry", geometry);
}

bool AppConfig::hasAskedFileAssociation() const
{
    return m_settings.value("FileAssociation/HasAsked", false).toBool();
}

void AppConfig::setHasAskedFileAssociation(bool asked)
{
    m_settings.setValue("FileAssociation/HasAsked", asked);
    m_settings.sync();
}

bool AppConfig::fileAssociationUserChoice() const
{
    return m_settings.value("FileAssociation/UserChoice", false).toBool();
}

void AppConfig::setFileAssociationUserChoice(bool choice)
{
    m_settings.setValue("FileAssociation/UserChoice", choice);
    m_settings.sync();
}
