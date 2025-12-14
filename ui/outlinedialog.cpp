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
    if (m_mode == AddMode) {
        setWindowTitle(tr("Add Outline"));
    } else {
        setWindowTitle(tr("Edit Outline"));
    }

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel* descLabel = new QLabel(this);
    if (m_mode == AddMode) {
        descLabel->setText(tr("Please enter outline information:"));
    } else {
        descLabel->setText(tr("Edit outline information:"));
    }
    QFont descFont = descLabel->font();
    descFont.setPointSize(10);
    descLabel->setFont(descFont);
    descLabel->setStyleSheet("color: #666666;");
    mainLayout->addWidget(descLabel);

    QFormLayout* formLayout = new QFormLayout();
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(tr("Enter outline title"));
    m_titleEdit->setMinimumWidth(300);
    QLabel* titleLabel = new QLabel(tr("Title:"), this);
    titleLabel->setMinimumWidth(60);
    formLayout->addRow(titleLabel, m_titleEdit);

    m_pageSpinBox = new QSpinBox(this);
    m_pageSpinBox->setMinimum(1);
    m_pageSpinBox->setMaximum(m_maxPage);
    m_pageSpinBox->setValue(1);
    m_pageSpinBox->setSuffix(tr(" page"));
    m_pageSpinBox->setMinimumWidth(150);
    QLabel* pageLabel = new QLabel(tr("Target Page:"), this);
    pageLabel->setMinimumWidth(60);
    formLayout->addRow(pageLabel, m_pageSpinBox);

    mainLayout->addLayout(formLayout);

    mainLayout->addStretch();

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);

    if (m_mode == AddMode) {
        m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Add"));
    } else {
        m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Save"));
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
