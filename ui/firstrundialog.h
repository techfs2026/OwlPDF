#ifndef FIRSTRUNDIALOG_H
#define FIRSTRUNDIALOG_H

#include <QDialog>
#include <QCheckBox>

class FirstRunDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FirstRunDialog(QWidget *parent = nullptr);

    bool shouldRegisterFileAssociation() const;
    bool shouldNotAskAgain() const;

private:
    QCheckBox* m_registerCheckBox;
    QCheckBox* m_dontAskCheckBox;

    void setupUI();
};

#endif
