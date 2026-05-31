#include "settingsdialog.h"
#include "appconfig.h"
#include "dictionaryconnector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QFrame>

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
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 主体：左侧分类列表 + 右侧堆叠面板
    QHBoxLayout* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    QListWidget* categoryList = new QListWidget(this);
    categoryList->setObjectName("settingsCategoryList");
    categoryList->setFixedWidth(150);
    categoryList->setFrameShape(QFrame::NoFrame);

    QStackedWidget* pages = new QStackedWidget(this);
    pages->setObjectName("settingsPages");

    // 新建一个带 FormLayout 的分类页，并把分类名登记到左侧列表
    auto addPage = [&](const QString& title) -> QFormLayout* {
        QWidget* page = new QWidget(this);
        QFormLayout* form = new QFormLayout(page);
        form->setContentsMargins(24, 24, 24, 24);
        form->setSpacing(12);
        form->setLabelAlignment(Qt::AlignRight);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        pages->addWidget(page);
        new QListWidgetItem(title, categoryList);
        return form;
    };

    // ---- 常规 ----
    QFormLayout* generalForm = addPage(tr("General"));
    m_rememberLastFile = new QCheckBox(tr("Reopen last session on startup"), this);
    generalForm->addRow(m_rememberLastFile);

    // ---- 缓存 ----
    QFormLayout* cacheForm = addPage(tr("Cache"));
    m_maxCacheSize = new QSpinBox(this);
    m_maxCacheSize->setRange(1, 100);
    m_maxCacheSize->setSuffix(tr(" pages"));
    cacheForm->addRow(tr("Page cache limit:"), m_maxCacheSize);

    m_preloadMargin = new QSpinBox(this);
    m_preloadMargin->setRange(0, 2000);
    m_preloadMargin->setSingleStep(50);
    m_preloadMargin->setSuffix(tr(" px"));
    cacheForm->addRow(tr("Preload margin:"), m_preloadMargin);

    QLabel* cacheHint = new QLabel(
        tr("Cache settings take effect the next time a document is opened."), this);
    cacheHint->setObjectName("settingsHint");
    cacheHint->setWordWrap(true);
    cacheForm->addRow(cacheHint);

    // ---- 性能 ----
    QFormLayout* perfForm = addPage(tr("Performance"));
    m_resizeDebounce = new QSpinBox(this);
    m_resizeDebounce->setRange(0, 1000);
    m_resizeDebounce->setSingleStep(10);
    m_resizeDebounce->setSuffix(tr(" ms"));
    perfForm->addRow(tr("Window resize debounce:"), m_resizeDebounce);

    // ---- OCR ----
    QFormLayout* ocrForm = addPage(tr("OCR"));
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

    // ---- 词典（OCR 查词调用外部词典）----
    QFormLayout* dictForm = addPage(tr("Dictionary"));

    QWidget* dictRow = new QWidget(this);
    QHBoxLayout* dictCmdRow = new QHBoxLayout(dictRow);
    dictCmdRow->setContentsMargins(0, 0, 0, 0);
    m_dictionaryCommand = new QLineEdit(this);
    m_dictionaryCommand->setPlaceholderText(tr("e.g. open -a GoldenDict {word}"));
    QPushButton* testDictBtn = new QPushButton(tr("Test"), this);
    dictCmdRow->addWidget(m_dictionaryCommand, 1);
    dictCmdRow->addWidget(testDictBtn);
    dictForm->addRow(tr("Command:"), dictRow);

    QLabel* dictHint = new QLabel(
        tr("Shell command used to look up the OCR-recognized word in an external "
           "dictionary. Use {word} as a placeholder for the query word; if omitted, "
           "the word is appended to the command."),
        this);
    dictHint->setObjectName("settingsHint");
    dictHint->setWordWrap(true);
    dictForm->addRow(dictHint);

    connect(testDictBtn, &QPushButton::clicked, this, [this]() {
        const QString cmd = m_dictionaryCommand->text().trimmed();
        if (cmd.isEmpty()) {
            QMessageBox::information(this, tr("Test Dictionary"),
                                     tr("Please enter a dictionary command first."));
            return;
        }
        QString error;
        // 用一个测试词实际调用，词典 UI 弹出即表示打通
        bool ok = DictionaryConnector::runCommand(cmd, QStringLiteral("test"), &error);
        if (ok) {
            QMessageBox::information(this, tr("Test Dictionary"),
                tr("Command launched. If your dictionary popped up with \"test\", it works."));
        } else {
            QMessageBox::warning(this, tr("Test Dictionary"),
                tr("Failed to run the command:\n%1").arg(error));
        }
    });

    bodyLayout->addWidget(categoryList);
    bodyLayout->addWidget(pages, 1);
    mainLayout->addLayout(bodyLayout, 1);

    // 底部按钮区
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this);
    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(20, 12, 20, 16);
    buttonRow->addWidget(buttons);
    mainLayout->addLayout(buttonRow);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applyToConfig();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &SettingsDialog::restoreDefaults);

    // 左右联动：选分类切换右侧页面
    connect(categoryList, &QListWidget::currentRowChanged,
            pages, &QStackedWidget::setCurrentIndex);
    categoryList->setCurrentRow(0);

    resize(580, 400);
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
    m_dictionaryCommand->setText(cfg.dictionaryCommand());
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
    cfg.setDictionaryCommand(m_dictionaryCommand->text().trimmed());
    cfg.save();
}

void SettingsDialog::restoreDefaults()
{
    // resetToDefaults 会立即落盘（只重置偏好项，不动会话/文件关联）
    AppConfig::instance().resetToDefaults();
    loadFromConfig();
}
