#include "stylemanager.h"
#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

StyleManager& StyleManager::instance()
{
    static StyleManager instance;
    return instance;
}

StyleManager::StyleManager()
    : m_currentTheme("light")
    , m_styleResourcePath(":/styles/resources/styles/")
{
}

StyleManager::~StyleManager()
{
}

void StyleManager::initialize()
{
    loadBuiltInThemes();

    setTheme("light");

    qDebug() << "[StyleManager] Initialized with theme:" << m_currentTheme;
}

void StyleManager::loadBuiltInThemes()
{
    ThemeConfig lightTheme;
    lightTheme.name = "light";
    lightTheme.isDark = false;

    lightTheme.primaryColor = QColor("#007AFF");
    lightTheme.secondaryColor = QColor("#5AC8FA");
    lightTheme.accentColor = QColor("#34C759");

    lightTheme.backgroundColor = QColor("#FAFAF8");
    lightTheme.surfaceColor = QColor("#FFFFFF");
    lightTheme.paperColor = QColor("#FCFCFA");

    lightTheme.textPrimary = QColor("#1C1C1E");
    lightTheme.textSecondary = QColor("#6B6B69");
    lightTheme.textDisabled = QColor("#C7C7C5");

    lightTheme.borderLight = QColor("#EBEBEA");
    lightTheme.borderMedium = QColor("#D5D5D3");
    lightTheme.borderDark = QColor("#A8A8A6");

    lightTheme.hoverBackground = QColor("#F5F5F3");
    lightTheme.pressedBackground = QColor("#EAEAE8");
    lightTheme.selectedBackground = QColor("#E8E8E6");

    lightTheme.successColor = QColor("#34C759");
    lightTheme.warningColor = QColor("#FF9500");
    lightTheme.errorColor = QColor("#FF3B30");
    lightTheme.infoColor = QColor("#007AFF");

    lightTheme.borderRadius = 6;
    lightTheme.fontSize = 13;
    lightTheme.fontFamily = "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', 'PingFang SC', 'Hiragino Sans GB', sans-serif";

    m_themes["light"] = lightTheme;

    ThemeConfig darkTheme;
    darkTheme.name = "dark";
    darkTheme.isDark = true;

    darkTheme.primaryColor = QColor("#0A84FF");
    darkTheme.secondaryColor = QColor("#64D2FF");
    darkTheme.accentColor = QColor("#30D158");

    darkTheme.backgroundColor = QColor("#1C1C1E");
    darkTheme.surfaceColor = QColor("#2C2C2E");
    darkTheme.paperColor = QColor("#242426");

    darkTheme.textPrimary = QColor("#FFFFFF");
    darkTheme.textSecondary = QColor("#EBEBF5");
    darkTheme.textDisabled = QColor("#545458");

    darkTheme.borderLight = QColor("#38383A");
    darkTheme.borderMedium = QColor("#48484A");
    darkTheme.borderDark = QColor("#636366");

    darkTheme.hoverBackground = QColor("#3A3A3C");
    darkTheme.pressedBackground = QColor("#48484A");
    darkTheme.selectedBackground = QColor("#0A84FF");

    darkTheme.successColor = QColor("#30D158");
    darkTheme.warningColor = QColor("#FF9F0A");
    darkTheme.errorColor = QColor("#FF453A");
    darkTheme.infoColor = QColor("#0A84FF");

    darkTheme.borderRadius = 6;
    darkTheme.fontSize = 13;
    darkTheme.fontFamily = lightTheme.fontFamily;

    m_themes["dark"] = darkTheme;
}

bool StyleManager::setTheme(const QString& themeName)
{
    if (!m_themes.contains(themeName)) {
        qWarning() << "[StyleManager] Theme not found:" << themeName;
        return false;
    }

    m_currentTheme = themeName;
    m_currentConfig = m_themes[themeName];

    m_cachedStyleSheets.clear();

    emit themeChanged(themeName);

    qDebug() << "[StyleManager] Theme changed to:" << themeName;
    return true;
}

void StyleManager::applyStyleToApplication(QObject* app)
{
    if (!app) return;

    QApplication* qapp = qobject_cast<QApplication*>(app);
    if (!qapp) return;

    QString fullStyleSheet = getFullStyleSheet();
    qapp->setStyleSheet(fullStyleSheet);

    qDebug() << "[StyleManager] Applied style to application";
}

void StyleManager::applyStyleToWidget(QWidget* widget, const QString& componentName)
{
    if (!widget) return;

    QString styleSheet;

    if (componentName.isEmpty()) {
        styleSheet = getFullStyleSheet();
    } else {
        styleSheet = getThemeStyleSheet() + "\n" + getComponentStyleSheet(componentName);
    }

    widget->setStyleSheet(styleSheet);

    qDebug() << "[StyleManager] Applied style to widget:" << componentName;
}

QString StyleManager::getFullStyleSheet()
{
    QString fullStyle;

    fullStyle += getThemeStyleSheet();
    fullStyle += "\n\n";

    QStringList components = {
        "mainwindow",
        "toolbar",
        "navigationpanel",
        "searchwidget",
        "pdfpagewidget",
        "statusbar",
        "scrollbar",
        "menu",
        "button",
    };

    for (const QString& component : components) {
        QString componentStyle = getComponentStyleSheet(component);
        if (!componentStyle.isEmpty()) {
            fullStyle += "/* ==================== " + component.toUpper() + " ==================== */\n";
            fullStyle += componentStyle;
            fullStyle += "\n\n";
        }
    }

    return fullStyle;
}

QString StyleManager::getThemeStyleSheet()
{
    QString themeFile = m_styleResourcePath + "themes/" + m_currentTheme + ".qss";
    QString styleSheet = loadStyleSheetFile(themeFile);

    if (styleSheet.isEmpty()) {
        qDebug() << "[StyleManager] Theme file not found, generating basic style";
        return generateBasicThemeStyle();
    }

    return processVariables(styleSheet, m_currentConfig);
}

QString StyleManager::getComponentStyleSheet(const QString& componentName)
{
    if (componentName.isEmpty()) return QString();

    if (m_cachedStyleSheets.contains(componentName)) {
        return m_cachedStyleSheets[componentName];
    }

    QString componentFile = m_styleResourcePath + "components/" + componentName + ".qss";
    QString styleSheet = loadStyleSheetFile(componentFile);

    QString processedStyle = processVariables(styleSheet, m_currentConfig);

    m_cachedStyleSheets[componentName] = processedStyle;

    return processedStyle;
}

QString StyleManager::loadStyleSheetFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        if (filePath.startsWith(":/")) {
            QString absolutePath = filePath;
            absolutePath.remove(0, 2);
            QFile absFile(absolutePath);
            if (!absFile.open(QFile::ReadOnly | QFile::Text)) {
                return QString();
            }
            QTextStream in(&absFile);
            return in.readAll();
        }
        return QString();
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    return content;
}

QString StyleManager::processVariables(const QString& styleSheet, const ThemeConfig& config)
{
    QString result = styleSheet;

    QMap<QString, QString> variables;

    variables["@primary-color"] = colorToHex(config.primaryColor);
    variables["@secondary-color"] = colorToHex(config.secondaryColor);
    variables["@accent-color"] = colorToHex(config.accentColor);

    variables["@background-color"] = colorToHex(config.backgroundColor);
    variables["@surface-color"] = colorToHex(config.surfaceColor);
    variables["@paper-color"] = colorToHex(config.paperColor);

    variables["@text-primary"] = colorToHex(config.textPrimary);
    variables["@text-secondary"] = colorToHex(config.textSecondary);
    variables["@text-disabled"] = colorToHex(config.textDisabled);

    variables["@border-light"] = colorToHex(config.borderLight);
    variables["@border-medium"] = colorToHex(config.borderMedium);
    variables["@border-dark"] = colorToHex(config.borderDark);

    variables["@hover-background"] = colorToHex(config.hoverBackground);
    variables["@pressed-background"] = colorToHex(config.pressedBackground);
    variables["@selected-background"] = colorToHex(config.selectedBackground);

    variables["@success-color"] = colorToHex(config.successColor);
    variables["@warning-color"] = colorToHex(config.warningColor);
    variables["@error-color"] = colorToHex(config.errorColor);
    variables["@info-color"] = colorToHex(config.infoColor);

    variables["@border-radius"] = QString::number(config.borderRadius) + "px";
    variables["@font-size"] = QString::number(config.fontSize) + "px";
    variables["@font-family"] = config.fontFamily;

    for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
        result.replace(it.key(), it.value());
    }

    return result;
}

QString StyleManager::colorToHex(const QColor& color) const
{
    return QString("#%1%2%3")
    .arg(color.red(), 2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.blue(), 2, 16, QChar('0'))
        .toUpper();
}

void StyleManager::reloadStyles()
{
    m_cachedStyleSheets.clear();
    emit themeChanged(m_currentTheme);
    qDebug() << "[StyleManager] Styles reloaded";
}

void StyleManager::registerTheme(const QString& themeName, const ThemeConfig& config)
{
    m_themes[themeName] = config;
    qDebug() << "[StyleManager] Registered theme:" << themeName;
}

QStringList StyleManager::availableThemes() const
{
    return m_themes.keys();
}

QColor StyleManager::getColor(const QString& colorName) const
{
    if (colorName == "primary") return m_currentConfig.primaryColor;
    if (colorName == "secondary") return m_currentConfig.secondaryColor;
    if (colorName == "accent") return m_currentConfig.accentColor;
    if (colorName == "background") return m_currentConfig.backgroundColor;
    if (colorName == "surface") return m_currentConfig.surfaceColor;
    if (colorName == "paper") return m_currentConfig.paperColor;
    if (colorName == "textPrimary") return m_currentConfig.textPrimary;
    if (colorName == "textSecondary") return m_currentConfig.textSecondary;
    if (colorName == "textDisabled") return m_currentConfig.textDisabled;
    if (colorName == "borderLight") return m_currentConfig.borderLight;
    if (colorName == "borderMedium") return m_currentConfig.borderMedium;
    if (colorName == "borderDark") return m_currentConfig.borderDark;
    if (colorName == "hover") return m_currentConfig.hoverBackground;
    if (colorName == "pressed") return m_currentConfig.pressedBackground;
    if (colorName == "selected") return m_currentConfig.selectedBackground;
    if (colorName == "success") return m_currentConfig.successColor;
    if (colorName == "warning") return m_currentConfig.warningColor;
    if (colorName == "error") return m_currentConfig.errorColor;
    if (colorName == "info") return m_currentConfig.infoColor;

    return QColor();
}

QString StyleManager::generateBasicThemeStyle() const
{
    return QString(R"(
    * {
        font-family: %1;
        font-size: %2px;
    }

    QWidget {
        background-color: %3;
        color: %4;
    }
    )")
        .arg(m_currentConfig.fontFamily)
        .arg(m_currentConfig.fontSize)
        .arg(colorToHex(m_currentConfig.backgroundColor))
        .arg(colorToHex(m_currentConfig.textPrimary));
}
