#include "annotationwidget.h"
#include "annotationmanager.h"
#include "pdfannotationhandler.h"
#include "themedicon.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QToolButton>
#include <QComboBox>
#include <QMenu>
#include <QColorDialog>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QSignalBlocker>

namespace {
constexpr int kIconSize = 18;
}

AnnotationWidget::AnnotationWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    updateColorSwatch();
    updateActionState();
    refresh();
}

QToolButton* AnnotationWidget::createIconButton(const QString& iconName,
                                                const QString& objectName,
                                                const QString& tooltip,
                                                bool checkable)
{
    QToolButton* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setIcon(ThemedIcon::toolButton(iconName, kIconSize));
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(tooltip);
    button->setCheckable(checkable);
    return button;
}

void AnnotationWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // —— 顶部控制条：固定 3 行，钢笔配置 / 橡皮配置 / 动作 分行，互不混淆 ——
    QWidget* bar = new QWidget(this);
    bar->setObjectName("annotationToolbar");
    bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    QVBoxLayout* rows = new QVBoxLayout(bar);
    rows->setContentsMargins(10, 8, 10, 8);
    rows->setSpacing(6);

    // 控件
    m_penButton = createIconButton("pen", "annotPenButton", tr("Pen"), true);
    m_eraserButton = createIconButton("eraser", "annotEraserButton", tr("Eraser"), true);

    m_colorButton = new QToolButton(bar);
    m_colorButton->setObjectName("annotColorButton");
    m_colorButton->setAutoRaise(true);
    m_colorButton->setFocusPolicy(Qt::NoFocus);
    m_colorButton->setCursor(Qt::PointingHandCursor);
    m_colorButton->setToolTip(tr("Pen color"));
    m_colorButton->setIconSize(QSize(kIconSize, kIconSize));

    m_penWidthCombo = new QComboBox(bar);
    m_penWidthCombo->setObjectName("annotPenWidthCombo");
    m_penWidthCombo->setToolTip(tr("Pen width"));
    m_penWidthCombo->addItem(tr("Fine"), 1.0);
    m_penWidthCombo->addItem(tr("Thin"), 2.0);
    m_penWidthCombo->addItem(tr("Medium"), 3.5);
    m_penWidthCombo->addItem(tr("Thick"), 6.0);
    m_penWidthCombo->setCurrentIndex(1);

    m_eraserSizeCombo = new QComboBox(bar);
    m_eraserSizeCombo->setObjectName("annotEraserSizeCombo");
    m_eraserSizeCombo->setToolTip(tr("Eraser size"));
    m_eraserSizeCombo->addItem(tr("Small"), 6.0);
    m_eraserSizeCombo->addItem(tr("Medium"), 12.0);
    m_eraserSizeCombo->addItem(tr("Large"), 24.0);
    m_eraserSizeCombo->setCurrentIndex(1);

    m_undoButton = createIconButton("undo", "annotUndoButton", tr("Undo (Ctrl+Z)"));
    m_redoButton = createIconButton("redo", "annotRedoButton", tr("Redo (Ctrl+Y)"));

    m_clearButton = createIconButton("trash", "annotClearButton", tr("Clear"));
    m_clearButton->setPopupMode(QToolButton::InstantPopup);
    QMenu* clearMenu = new QMenu(this);
    m_clearPageItem = clearMenu->addAction(tr("Clear current page"));
    m_clearAllItem  = clearMenu->addAction(tr("Clear all pages"));
    m_clearButton->setMenu(clearMenu);

    // 第 1 行：钢笔 + 粗细 + 颜色
    QHBoxLayout* penRow = new QHBoxLayout();
    penRow->setSpacing(6);
    penRow->addWidget(m_penButton);
    penRow->addWidget(m_colorButton);
    penRow->addWidget(m_penWidthCombo, 1);
    rows->addLayout(penRow);

    // 第 2 行：橡皮 + 大小
    QHBoxLayout* eraserRow = new QHBoxLayout();
    eraserRow->setSpacing(6);
    eraserRow->addWidget(m_eraserButton);
    eraserRow->addWidget(m_eraserSizeCombo, 1);
    rows->addLayout(eraserRow);

    // 第 3 行：撤销 / 重做 / 清空
    QHBoxLayout* actionRow = new QHBoxLayout();
    actionRow->setSpacing(6);
    actionRow->addWidget(m_undoButton);
    actionRow->addWidget(m_redoButton);
    actionRow->addWidget(m_clearButton);
    actionRow->addStretch();
    rows->addLayout(actionRow);

    mainLayout->addWidget(bar);

    // —— 页列表 ——
    m_emptyHint = new QLabel(tr("No annotations yet"), this);
    m_emptyHint->setObjectName("annotationEmptyHint");
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);

    m_list = new QListWidget(this);
    m_list->setObjectName("annotationListWidget");
    m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    mainLayout->addWidget(m_emptyHint);
    mainLayout->addWidget(m_list, 1);

    // —— 连接 ——
    connect(m_penButton, &QToolButton::toggled, this, &AnnotationWidget::onPenToggled);
    connect(m_eraserButton, &QToolButton::toggled, this, &AnnotationWidget::onEraserToggled);
    connect(m_penWidthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnnotationWidget::onPenWidthChanged);
    connect(m_colorButton, &QToolButton::clicked, this, &AnnotationWidget::choosePenColor);
    connect(m_eraserSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnnotationWidget::onEraserSizeChanged);
    connect(m_undoButton, &QToolButton::clicked, this, [this]() {
        if (m_manager) m_manager->undo();
    });
    connect(m_redoButton, &QToolButton::clicked, this, [this]() {
        if (m_manager) m_manager->redo();
    });
    connect(m_clearPageItem, &QAction::triggered, this, &AnnotationWidget::clearCurrentPage);
    connect(m_clearAllItem, &QAction::triggered, this, &AnnotationWidget::clearAllPages);

    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (item) emit pageJumpRequested(item->data(Qt::UserRole).toInt());
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) emit pageJumpRequested(item->data(Qt::UserRole).toInt());
    });
}

void AnnotationWidget::setManager(AnnotationManager* manager)
{
    if (m_manager == manager) {
        return;
    }
    if (m_managerUndoConn) disconnect(m_managerUndoConn);
    if (m_managerChangeConn) disconnect(m_managerChangeConn);

    m_manager = manager;

    if (m_manager) {
        m_managerUndoConn = connect(m_manager, &AnnotationManager::undoStateChanged,
                                    this, &AnnotationWidget::updateActionState);
        m_managerChangeConn = connect(m_manager, &AnnotationManager::annotationsChanged,
                                      this, [this](int) { refresh(); updateActionState(); });
    }
    refresh();
    updateActionState();
}

void AnnotationWidget::setHandler(PDFAnnotationHandler* handler)
{
    m_handler = handler;
    // 把当前控件状态（工具/配置）施加到新文档的 handler —— 全局工具的核心。
    applyConfigToHandler();
}

void AnnotationWidget::setCurrentPage(int pageIndex)
{
    m_currentPage = pageIndex;
    updateActionState();
}

void AnnotationWidget::activatePen()
{
    if (!m_penButton->isChecked()) {
        m_penButton->setChecked(true);   // 触发 onPenToggled → setTool(Pen)
    } else {
        applyConfigToHandler();
    }
}

void AnnotationWidget::applyConfigToHandler()
{
    if (!m_handler) {
        return;
    }
    m_handler->setPenColor(m_penColor);
    m_handler->setPenWidth(m_penWidthCombo->currentData().toReal());
    m_handler->setEraserRadius(m_eraserSizeCombo->currentData().toReal());

    AnnotTool tool = m_penButton->isChecked()    ? AnnotTool::Pen
                     : m_eraserButton->isChecked() ? AnnotTool::Eraser
                                                   : AnnotTool::None;
    m_handler->setTool(tool);
}

void AnnotationWidget::onPenToggled(bool checked)
{
    if (checked) {
        QSignalBlocker be(m_eraserButton);
        m_eraserButton->setChecked(false);
    }
    if (m_handler) {
        AnnotTool tool = checked ? AnnotTool::Pen
                                 : (m_eraserButton->isChecked() ? AnnotTool::Eraser : AnnotTool::None);
        m_handler->setTool(tool);
    }
}

void AnnotationWidget::onEraserToggled(bool checked)
{
    if (checked) {
        QSignalBlocker bp(m_penButton);
        m_penButton->setChecked(false);
    }
    if (m_handler) {
        AnnotTool tool = checked ? AnnotTool::Eraser
                                 : (m_penButton->isChecked() ? AnnotTool::Pen : AnnotTool::None);
        m_handler->setTool(tool);
    }
}

void AnnotationWidget::onPenWidthChanged(int index)
{
    if (m_handler) {
        m_handler->setPenWidth(m_penWidthCombo->itemData(index).toReal());
    }
}

void AnnotationWidget::choosePenColor()
{
    QColor c = QColorDialog::getColor(m_penColor, this, tr("Pen Color"));
    if (!c.isValid()) {
        return;
    }
    m_penColor = c;
    updateColorSwatch();
    if (m_handler) {
        m_handler->setPenColor(m_penColor);
    }
    if (!m_penButton->isChecked()) {
        m_penButton->setChecked(true);   // 选色通常意味着想继续画
    }
}

void AnnotationWidget::onEraserSizeChanged(int index)
{
    if (m_handler) {
        m_handler->setEraserRadius(m_eraserSizeCombo->itemData(index).toReal());
    }
}

void AnnotationWidget::clearCurrentPage()
{
    if (m_manager) {
        m_manager->clearPage(m_currentPage);
    }
}

void AnnotationWidget::clearAllPages()
{
    if (m_manager) {
        m_manager->clearAll();
    }
}

void AnnotationWidget::updateActionState()
{
    const bool hasManager = m_manager != nullptr;
    m_undoButton->setEnabled(hasManager && m_manager->canUndo());
    m_redoButton->setEnabled(hasManager && m_manager->canRedo());
    m_clearButton->setEnabled(hasManager && !m_manager->isEmpty());
    if (m_clearPageItem) {
        m_clearPageItem->setEnabled(hasManager && m_manager->strokeCountForPage(m_currentPage) > 0);
    }
    if (m_clearAllItem) {
        m_clearAllItem->setEnabled(hasManager && !m_manager->isEmpty());
    }
}

void AnnotationWidget::updateColorSwatch()
{
    QPixmap pm(kIconSize, kIconSize);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(0, 0, 0, 60), 1));
        p.setBrush(m_penColor);
        p.drawEllipse(1, 1, kIconSize - 3, kIconSize - 3);
    }
    m_colorButton->setIcon(QIcon(pm));
}

void AnnotationWidget::refresh()
{
    m_list->clear();

    if (!m_manager) {
        m_emptyHint->setVisible(true);
        m_list->setVisible(false);
        return;
    }

    const QList<int> pages = m_manager->pagesWithAnnotations();
    for (int page : pages) {
        const int count = m_manager->strokeCountForPage(page);
        QListWidgetItem* item = new QListWidgetItem(
            tr("Page %1  ·  %n stroke(s)", nullptr, count).arg(page + 1), m_list);
        item->setData(Qt::UserRole, page);
    }

    const bool empty = pages.isEmpty();
    m_emptyHint->setVisible(empty);
    m_list->setVisible(!empty);
}
