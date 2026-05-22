#include "searchwidget.h"
#include "pdfdocumentsession.h"
#include "pdfdocumentstate.h"
#include "pdfinteractionhandler.h"
#include "themedicon.h"
#include "stylemanager.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>

namespace {
constexpr int kIconSize = 18;
}

SearchWidget::SearchWidget(PDFDocumentSession* session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    // 纯 QWidget 默认不绘制 QSS 的 background/border/padding，需开启此属性
    setAttribute(Qt::WA_StyledBackground, true);

    setupUI();
    setupConnections();
    updateUI();
}

QToolButton* SearchWidget::createIconButton(const QString& iconName,
                                            const QString& objectName,
                                            const QString& tooltip)
{
    QToolButton* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setIcon(ThemedIcon::toolButton(iconName, kIconSize));
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(tooltip);
    return button;
}

void SearchWidget::setupUI()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);   // 外边距交由 QSS 的 padding 控制
    layout->setSpacing(6);

    // 搜索输入框：前导放大镜 + 尾随清除，均为 QLineEdit 内嵌 action
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("searchLineEdit");
    m_searchEdit->setPlaceholderText(tr("Find in document"));

    const QColor iconColor = StyleManager::instance().getColor("textSecondary");
    m_searchEdit->addAction(ThemedIcon::colored("search", iconColor, kIconSize),
                            QLineEdit::LeadingPosition);

    m_clearAction = m_searchEdit->addAction(
        ThemedIcon::colored("close-file", iconColor, kIconSize),
        QLineEdit::TrailingPosition);
    m_clearAction->setToolTip(tr("Clear"));
    m_clearAction->setVisible(false);

    // 搜索历史自动补全：输入时弹出匹配的历史记录
    m_historyModel = new QStringListModel(this);
    m_historyCompleter = new QCompleter(m_historyModel, this);
    m_historyCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_historyCompleter->setFilterMode(Qt::MatchContains);
    m_historyCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_historyCompleter->popup()->setObjectName("searchHistoryPopup");
    m_searchEdit->setCompleter(m_historyCompleter);

    layout->addWidget(m_searchEdit, 1);

    // 上一个 / 下一个匹配
    m_previousButton = createIconButton("left-arrow", "previousButton", tr("Previous match"));
    m_nextButton     = createIconButton("right-arrow", "nextButton", tr("Next match"));
    m_previousButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    layout->addWidget(m_previousButton);
    layout->addWidget(m_nextButton);

    // 匹配计数
    m_matchLabel = new QLabel(this);
    m_matchLabel->setObjectName("matchLabel");
    m_matchLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_matchLabel);

    // 搜索选项：按钮 + 下拉菜单
    m_optionsButton = createIconButton("search-options", "optionsButton", tr("Search options"));
    m_optionsButton->setCheckable(true);   // 仅作"有选项启用"的视觉提示，由代码控制
    m_optionsButton->setPopupMode(QToolButton::InstantPopup);

    QMenu* optionsMenu = new QMenu(this);
    m_caseSensitiveAction = optionsMenu->addAction(tr("Case sensitive"));
    m_caseSensitiveAction->setCheckable(true);
    m_wholeWordsAction = optionsMenu->addAction(tr("Whole words"));
    m_wholeWordsAction->setCheckable(true);
    m_optionsButton->setMenu(optionsMenu);
    layout->addWidget(m_optionsButton);

    layout->addStretch();

    // 关闭
    m_closeButton = createIconButton("close-file", "closeButton", tr("Close (Esc)"));
    layout->addWidget(m_closeButton);
}

void SearchWidget::setupConnections()
{
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SearchWidget::performSearch);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SearchWidget::updateClearButton);

    // 从历史补全列表选中一项即填入并立即搜索
    connect(m_historyCompleter, QOverload<const QString&>::of(&QCompleter::activated),
            this, [this](const QString& text) {
        m_searchEdit->setText(text);
        performSearch();
    });

    connect(m_clearAction, &QAction::triggered, this, [this]() {
        m_searchEdit->clear();
        m_searchEdit->setFocus();
        performSearch();   // 空查询 → 取消搜索并刷新界面
    });

    connect(m_previousButton, &QToolButton::clicked, this, &SearchWidget::findPrevious);
    connect(m_nextButton, &QToolButton::clicked, this, &SearchWidget::findNext);
    connect(m_closeButton, &QToolButton::clicked, this, &SearchWidget::closeRequested);

    connect(m_caseSensitiveAction, &QAction::toggled, this, &SearchWidget::performSearch);
    connect(m_wholeWordsAction, &QAction::toggled, this, &SearchWidget::performSearch);

    connect(m_session, &PDFDocumentSession::searchCompleted,
            this, &SearchWidget::onSearchCompleted);
    connect(m_session, &PDFDocumentSession::searchProgressUpdated,
            this, &SearchWidget::onSearchProgress);
    connect(m_session, &PDFDocumentSession::searchCancelled, this, [this]() {
        m_isSearching = false;
        updateUI();
    });
}

void SearchWidget::showAndFocus()
{
    refreshHistory();
    show();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

QString SearchWidget::searchText() const
{
    return m_searchEdit->text();
}

void SearchWidget::performSearch()
{
    const bool caseSensitive = m_caseSensitiveAction->isChecked();
    const bool wholeWords    = m_wholeWordsAction->isChecked();
    // 任一选项启用时高亮选项按钮（命中 QSS 的 #optionsButton:checked）
    m_optionsButton->setChecked(caseSensitive || wholeWords);

    const QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) {
        m_session->cancelSearch();
        m_isSearching = false;
        updateUI();
        return;
    }

    if (m_isSearching) {
        m_session->cancelSearch();
    }

    const int startPage = m_session->state()->currentPage();
    m_session->startSearch(query, caseSensitive, wholeWords, startPage);

    if (m_session->interactionHandler()) {
        m_session->interactionHandler()->addSearchHistory(query);
        refreshHistory();
    }

    m_isSearching = true;
    updateUI();
}

void SearchWidget::findNext()
{
    const SearchResult result = m_session->findNext();
    if (result.isValid()) {
        navigateToResult(result);
        updateUI();
    }
}

void SearchWidget::findPrevious()
{
    const SearchResult result = m_session->findPrevious();
    if (result.isValid()) {
        navigateToResult(result);
        updateUI();
    }
}

void SearchWidget::updateUI()
{
    const PDFDocumentState* state = m_session->state();
    const int totalMatches = state->searchTotalMatches();
    const int currentIndex = state->searchCurrentMatchIndex();

    const bool hasResults = totalMatches > 0;
    m_previousButton->setEnabled(hasResults && !m_isSearching);
    m_nextButton->setEnabled(hasResults && !m_isSearching);

    if (m_isSearching) {
        m_matchLabel->setText(tr("Searching..."));
    } else if (totalMatches == 0) {
        m_matchLabel->setText(tr("No matches"));
    } else {
        m_matchLabel->setText(tr("%1 / %2")
                                  .arg(currentIndex + 1)
                                  .arg(totalMatches));
    }
}

void SearchWidget::updateClearButton()
{
    m_clearAction->setVisible(!m_searchEdit->text().isEmpty());
}

void SearchWidget::refreshHistory()
{
    if (!m_session || !m_session->interactionHandler()) {
        return;
    }
    m_historyModel->setStringList(m_session->interactionHandler()->getSearchHistory(20));
}

void SearchWidget::onSearchCompleted(const QString& query, int totalMatches)
{
    Q_UNUSED(query);

    m_isSearching = false;
    updateUI();

    if (totalMatches > 0) {
        const SearchResult result = m_session->findNext();
        if (result.isValid()) {
            navigateToResult(result);
            updateUI();
        }
    }
}

void SearchWidget::onSearchProgress(int currentPage, int totalPages, int matchCount)
{
    Q_UNUSED(matchCount);

    const int percent = (totalPages > 0)
                            ? qBound(0, currentPage * 100 / totalPages, 100)
                            : 0;
    m_matchLabel->setText(tr("%1%").arg(percent));
}

void SearchWidget::navigateToResult(const SearchResult& result)
{
    if (!result.isValid()) {
        return;
    }

    const PDFDocumentState* state = m_session->state();
    if (state->currentPage() != result.pageIndex) {
        m_session->goToPage(result.pageIndex);
    }

    emit searchResultNavigated(result);
}

void SearchWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}
