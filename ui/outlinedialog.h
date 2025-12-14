#ifndef OUTLINEDIALOG_H
#define OUTLINEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>


class OutlineDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        AddMode,
        EditMode
    };

    explicit OutlineDialog(Mode mode, int maxPage, QWidget* parent = nullptr);
    ~OutlineDialog();

    void setTitle(const QString& title);
    QString title() const;

    void setPageIndex(int pageIndex);

    int pageIndex() const;

    bool validate();

private:
    void setupUI();

private slots:
    void onAccepted();

private:
    Mode m_mode;
    int m_maxPage;

    QLineEdit* m_titleEdit;
    QSpinBox* m_pageSpinBox;
    QDialogButtonBox* m_buttonBox;
};

#endif // OUTLINEDIALOG_H
