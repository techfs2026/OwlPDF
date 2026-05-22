#include "settingsdialog.h"
#include "appconfig.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName("settingsDialog");
    setWindowTitle(tr("Settings"));
    setModal(true);

    buildUI();
    loadFromConfig();
}

void SettingsDialog::buildUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // ---- 常规 ----
    QGroupBox* generalGroup = new QGroupBox(tr("General"), this);
    QFormLayout* generalForm = new QFormLayout(generalGroup);
    m_rememberLastFile = new QCheckBox(tr("Reopen last session on startup"), this);
    generalForm->addRow(m_rememberLastFile);

    // ---- 缓存 ----
    QGroupBox* cacheGroup = new QGroupBox(tr("Cache"), this);
    QFormLayout* cacheForm = new QFormLayout(cacheGroup);

    m_maxCacheSize = new QSpinBox(this);
    m_maxCacheSize->setRange(1, 100);
    m_maxCacheSize->setSuffix(tr(" pages"));
    cacheForm->addRow(tr("Page cache limit:"), m_maxCacheSize);

    m_preloadMargin = new QSpinBox(this);
    m_preloadMargin->setRange(0, 2000);
    m_preloadMargin->setSingleStep(50);
    m_preloadMargin->setSuffix(tr(" px"));
    cacheForm->addRow(tr("Preload margin:"), m_preloadMargin);

    // ---- 性能 ----
    QGroupBox* perfGroup = new QGroupBox(tr("Performance"), this);
    QFormLayout* perfForm = new QFormLayout(perfGroup);

    m_resizeDebounce = new QSpinBox(this);
    m_resizeDebounce->setRange(0, 1000);
    m_resizeDebounce->setSingleStep(10);
    m_resizeDebounce->setSuffix(tr(" ms"));
    perfForm->addRow(tr("Window resize debounce:"), m_resizeDebounce);

    // ---- OCR ----
    QGroupBox* ocrGroup = new QGroupBox(tr("OCR"), this);
    QFormLayout* ocrForm = new QFormLayout(ocrGroup);

    m_ocrDebounce = new QSpinBox(this);
    m_ocrDebounce->setRange(0, 2000);
    m_ocrDebounce->setSingleStep(50);
    m_ocrDebounce->setSuffix(tr(" ms"));
    ocrForm->addRow(tr("Hover lookup debounce:"), m_ocrDebounce);

    m_ocrRegionSize = new QSpinBox(this);
    m_ocrRegionSize->setRange(50, 600);
    m_ocrRegionSize->setSingleStep(20);
    m_ocrRegionSize->setSuffix(tr(" px"));
    ocrForm->addRow(tr("Hover capture region:"), m_ocrRegionSize);

    mainLayout->addWidget(generalGroup);
    mainLayout->addWidget(cacheGroup);
    mainLayout->addWidget(perfGroup);
    mainLayout->addWidget(ocrGroup);

    QLabel* hint = new QLabel(
        tr("Cache settings take effect the next time a document is opened."), this);
    hint->setObjectName("settingsHint");
    hint->setWordWrap(true);
    mainLayout->addWidget(hint);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applyToConfig();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &SettingsDialog::restoreDefaults);
}

void SettingsDialog::loadFromConfig()
{
    AppConfig& cfg = AppConfig::instance();
    m_rememberLastFile->setChecked(cfg.rememberLastFile());
    m_maxCacheSize->setValue(cfg.maxCacheSize());
    m_preloadMargin->setValue(cfg.preloadMargin());
    m_resizeDebounce->setValue(cfg.resizeDebounceDelay());
    m_ocrDebounce->setValue(cfg.ocrDebounceDelay());
    m_ocrRegionSize->setValue(cfg.ocrHoverRegionSize());
}

void SettingsDialog::applyToConfig()
{
    AppConfig& cfg = AppConfig::instance();
    cfg.setRememberLastFile(m_rememberLastFile->isChecked());
    cfg.setMaxCacheSize(m_maxCacheSize->value());
    cfg.setPreloadMargin(m_preloadMargin->value());
    cfg.setResizeDebounceDelay(m_resizeDebounce->value());
    cfg.setOcrDebounceDelay(m_ocrDebounce->value());
    cfg.setOcrHoverRegionSize(m_ocrRegionSize->value());
    cfg.save();
}

void SettingsDialog::restoreDefaults()
{
    // resetToDefaults 会立即落盘（只重置偏好项，不动会话/文件关联）
    AppConfig::instance().resetToDefaults();
    loadFromConfig();
}
