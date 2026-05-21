#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include <QString>
#include <QMap>
#include <QColor>
#include <QObject>

struct ThemeConfig {
    QColor primaryColor;
    QColor secondaryColor;
    QColor accentColor;

    QColor backgroundColor;
    QColor surfaceColor;
    QColor paperColor;

    QColor textPrimary;
    QColor textSecondary;
    QColor textDisabled;

    QColor borderLight;
    QColor borderMedium;
    QColor borderDark;

    QColor hoverBackground;
    QColor pressedBackground;
    QColor selectedBackground;

    QColor successColor;
    QColor warningColor;
    QColor errorColor;
    QColor infoColor;

    QString name;
    bool isDark;
    int borderRadius;
    int fontSize;
    QString fontFamily;

    // 间距梯度（4px 基准）
    int space1 = 4;
    int space2 = 8;
    int space3 = 12;
    int space4 = 16;
    int space5 = 24;

    // 圆角梯度
    int radiusSm = 4;
    int radiusMd = 6;
    int radiusLg = 10;

    // 字号梯度
    int fontSizeSm = 12;
    int fontSizeBase = 13;
    int fontSizeLg = 15;
};

class StyleManager : public QObject
{
    Q_OBJECT

public:
    static StyleManager& instance();

    void initialize();

    bool setTheme(const QString& themeName);

    QString currentTheme() const { return m_currentTheme; }

    ThemeConfig currentConfig() const { return m_currentConfig; }

    void applyStyleToApplication(QObject* app);

    void applyStyleToWidget(QWidget* widget, const QString& componentName = QString());

    QString getFullStyleSheet();

    QString getThemeStyleSheet();

    QString getComponentStyleSheet(const QString& componentName);

    void reloadStyles();

    void registerTheme(const QString& themeName, const ThemeConfig& config);

    QStringList availableThemes() const;

    bool isDarkTheme() const { return m_currentConfig.isDark; }

    QColor getColor(const QString& colorName) const;

signals:
    void themeChanged(const QString& themeName);

private:
    StyleManager();
    ~StyleManager();
    StyleManager(const StyleManager&) = delete;
    StyleManager& operator=(const StyleManager&) = delete;

    void loadBuiltInThemes();

    ThemeConfig loadThemeConfig(const QString& themeName);

    QString loadStyleSheetFile(const QString& filePath);

    QString processVariables(const QString& styleSheet, const ThemeConfig& config);

    QString colorToHex(const QColor& color) const;

    QString generateBasicThemeStyle() const;

private:
    QString m_currentTheme;
    ThemeConfig m_currentConfig;
    QMap<QString, ThemeConfig> m_themes;
    QMap<QString, QString> m_cachedStyleSheets;
    QString m_styleResourcePath;
};

#endif // STYLEMANAGER_H