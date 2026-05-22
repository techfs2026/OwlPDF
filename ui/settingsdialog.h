#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QSpinBox;

// 应用设置对话框：把 AppConfig 里的可调项暴露给用户。
// macOS 经 QAction::PreferencesRole 进应用菜单，其它平台进 Tools 菜单。
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private:
    void buildUI();
    void loadFromConfig();
    void applyToConfig();
    void restoreDefaults();

    QCheckBox* m_rememberLastFile = nullptr;
    QSpinBox*  m_maxCacheSize     = nullptr;
    QSpinBox*  m_preloadMargin    = nullptr;
    QSpinBox*  m_resizeDebounce   = nullptr;
    QSpinBox*  m_ocrDebounce      = nullptr;
    QSpinBox*  m_ocrRegionSize    = nullptr;
};

#endif // SETTINGSDIALOG_H
