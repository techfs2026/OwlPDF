#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QWidget>
#include "datastructure.h"

class QLineEdit;
class QToolButton;
class QLabel;
class QAction;
class QKeyEvent;
class QCompleter;
class QStringListModel;
class PDFDocumentSession;

// 文档内搜索栏：图标化紧凑布局。
// 视觉样式全部由 searchwidget.qss 驱动（靠 objectName 命中），
// 图标统一走 ThemedIcon —— 单色 SVG，颜色跟随 StyleManager 主题。
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
    void updateClearButton();
    void refreshHistory();
    void navigateToResult(const SearchResult& result);

    // 统一构造工具栏风格的图标按钮（ThemedIcon SVG + QSS objectName）
    QToolButton* createIconButton(const QString& iconName,
                                  const QString& objectName,
                                  const QString& tooltip);

    PDFDocumentSession* m_session;

    QLineEdit*   m_searchEdit     = nullptr;
    QAction*     m_clearAction    = nullptr;
    QToolButton* m_previousButton = nullptr;
    QToolButton* m_nextButton     = nullptr;
    QLabel*      m_matchLabel     = nullptr;
    QToolButton* m_optionsButton  = nullptr;
    QToolButton* m_closeButton    = nullptr;

    QAction* m_caseSensitiveAction = nullptr;
    QAction* m_wholeWordsAction    = nullptr;

    // 搜索历史自动补全：模型数据来自 SearchManager 的历史记录
    QCompleter*       m_historyCompleter = nullptr;
    QStringListModel* m_historyModel     = nullptr;

    bool m_isSearching = false;
};

#endif // SEARCHWIDGET_H
