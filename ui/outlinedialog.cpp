#include "outlinedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QPushButton>

OutlineDialog::OutlineDialog(Mode mode, int maxPage, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_maxPage(maxPage)
    , m_titleEdit(nullptr)
    , m_pageSpinBox(nullptr)
    , m_buttonBox(nullptr)
{
    setupUI();

    setModal(true);
    setMinimumWidth(400);

    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &OutlineDialog::onAccepted);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

OutlineDialog::~OutlineDialog()
{
}

void OutlineDialog::setupUI()
{
    // objectName 供 dialog.qss 选择器命中（如需对本对话框做差异化）
    setObjectName("outlineDialog");

    if (m_mode == AddMode) {
        setWindowTitle(tr("Add Outline"));
    } else {
        setWindowTitle(tr("Edit Outline"));
    }

    // 布局间距对齐 token 数值梯度（@space-4 = 16, @space-5 = 24）。
    // QLayout 的 spacing/margin 无法由 qss 设置，故在此使用，但取值与
    // token 体系保持一致，不引入魔法数字。
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);                     // @space-4
    mainLayout->setContentsMargins(24, 24, 24, 24); // @space-5

    // 说明文字：用 role="caption" 声明语义，颜色/字号由 dialog.qss 决定
    QLabel* descLabel = new QLabel(this);
    descLabel->setProperty("role", "caption");
    if (m_mode == AddMode) {
        descLabel->setText(tr("Please enter outline information:"));
    } else {
        descLabel->setText(tr("Edit outline information:"));
    }
    mainLayout->addWidget(descLabel);

    QFormLayout* formLayout = new QFormLayout();
    formLayout->setSpacing(12);                     // @space-3
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(tr("Enter outline title"));
    QLabel* titleLabel = new QLabel(tr("Title:"), this);
    formLayout->addRow(titleLabel, m_titleEdit);

    m_pageSpinBox = new QSpinBox(this);
    m_pageSpinBox->setMinimum(1);
    m_pageSpinBox->setMaximum(m_maxPage);
    m_pageSpinBox->setValue(1);
    m_pageSpinBox->setSuffix(tr(" page"));
    QLabel* pageLabel = new QLabel(tr("Target Page:"), this);
    formLayout->addRow(pageLabel, m_pageSpinBox);

    mainLayout->addLayout(formLayout);

    mainLayout->addStretch();

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);

    QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok);
    // 确定按钮为主操作，用 variant="primary" 命中 button.qss 的主色变体
    okButton->setProperty("variant", "primary");
    if (m_mode == AddMode) {
        okButton->setText(tr("Add"));
    } else {
        okButton->setText(tr("Save"));
    }
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

    mainLayout->addWidget(m_buttonBox);

    setTabOrder(m_titleEdit, m_pageSpinBox);
    setTabOrder(m_pageSpinBox, m_buttonBox);

    m_titleEdit->setFocus();
}

void OutlineDialog::setTitle(const QString& title)
{
    m_titleEdit->setText(title);
}

QString OutlineDialog::title() const
{
    return m_titleEdit->text().trimmed();
}

void OutlineDialog::setPageIndex(int pageIndex)
{
    m_pageSpinBox->setValue(pageIndex + 1);
}

int OutlineDialog::pageIndex() const
{
    return m_pageSpinBox->value() - 1;
}

bool OutlineDialog::validate()
{
    QString title = m_titleEdit->text().trimmed();

    if (title.isEmpty()) {
        QMessageBox::warning(this, tr("Input Error"),
                             tr("Outline title cannot be empty!"));
        m_titleEdit->setFocus();
        return false;
    }

    if (title.length() > 200) {
        QMessageBox::warning(this, tr("Input Error"),
                             tr("Outline title too long (max 200 characters)!"));
        m_titleEdit->setFocus();
        return false;
    }

    return true;
}

void OutlineDialog::onAccepted()
{
    if (validate()) {
        accept();
    }
}