#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include "datastructure.h"

class PDFDocumentSession;

class SearchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchWidget(PDFDocumentSession* session, QWidget* parent = nullptr);

    void showAndFocus();
    QString searchText() const;

public slots:
    void findNext();
    void findPrevious();

signals:
    void closeRequested();

    void searchResultNavigated(const SearchResult& result);

private slots:
    void performSearch();
    void onSearchCompleted(const QString& query, int totalMatches);
    void onSearchProgress(int currentPage, int totalPages, int matchCount);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupUI();
    void setupConnections();
    void updateUI();
    void navigateToResult(const SearchResult& result);

private:
    PDFDocumentSession* m_session;

    QComboBox* m_searchCombo;
    QPushButton* m_previousButton;
    QPushButton* m_nextButton;
    QLabel* m_matchLabel;
    QCheckBox* m_caseSensitiveCheck;
    QCheckBox* m_wholeWordsCheck;
    QToolButton* m_closeButton;

    bool m_isSearching;
};

#endif // SEARCHWIDGET_H
