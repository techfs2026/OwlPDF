#include "firstrundialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

FirstRunDialog::FirstRunDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle(tr("Welcome"));
    setModal(true);
    setMinimumWidth(500);
    setMaximumWidth(600);
    adjustSize();
}

void FirstRunDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel* welcomeLabel = new QLabel(tr("Thank you for using MuQt!"), this);
    QFont font = welcomeLabel->font();
    font.setPointSize(12);
    font.setBold(true);
    welcomeLabel->setFont(font);
    mainLayout->addWidget(welcomeLabel);

    mainLayout->addSpacing(5);

    QLabel* infoLabel = new QLabel(
        tr("For a better user experience, we can associate PDF files with this application.\n"
           "This allows you to open PDF files directly by double-clicking them.\n\n"
           "You can change this setting at any time from the Tools menu."),
        this
        );
    infoLabel->setWordWrap(true);
    infoLabel->setMinimumHeight(80);
    mainLayout->addWidget(infoLabel);

    mainLayout->addSpacing(5);

    m_registerCheckBox = new QCheckBox(tr("Associate PDF files (Recommended)"), this);
    m_registerCheckBox->setChecked(true);
    mainLayout->addWidget(m_registerCheckBox);

    m_dontAskCheckBox = new QCheckBox(tr("Don't ask again"), this);
    mainLayout->addWidget(m_dontAskCheckBox);

    mainLayout->addStretch();

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* okButton = new QPushButton(tr("OK"), this);
    okButton->setDefault(true);
    okButton->setMinimumWidth(80);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(okButton);

    mainLayout->addLayout(buttonLayout);
}

bool FirstRunDialog::shouldRegisterFileAssociation() const
{
    return m_registerCheckBox->isChecked();
}

bool FirstRunDialog::shouldNotAskAgain() const
{
    return m_dontAskCheckBox->isChecked();
}
