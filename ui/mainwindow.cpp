#include "mainwindow.h"
#include "pdfdocumenttab.h"
#include "dictionaryconnector.h"
#include "ocrstatusindicator.h"
#include "ocrmanager.h"
#include "chinesetokenizer.h"
#include "appconfig.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QTabWidget>
#include <QTabBar>
#include <QApplication>
#include <QFileInfo>
#include <QCloseEvent>
#include <QDockWidget>
#include <QActionGroup>
#include <QShortcut>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_navigationDock(nullptr)
    , m_toolBar(nullptr)
    , m_pageSpinBox(nullptr)
    , m_zoomComboBox(nullptr)
    , m_statusLabel(nullptr)
    , m_pageLabel(nullptr)
    , m_zoomLabel(nullptr)
    , m_ocrInitialized(false)
{
    setWindowTitle(tr("MuQt"));
    resize(AppConfig::instance().defaultWindowSize());

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setUsesScrollButtons(true);
    m_tabWidget->tabBar()->setExpanding(false);

    setCentralWidget(m_tabWidget);

    m_navigationDock = new QDockWidget(tr("Navigation"), this);
    m_navigationDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_navigationDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::LeftDockWidgetArea, m_navigationDock);
    m_navigationDock->setVisible(false);

    createActions();
    createMenuBar();
    createToolBar();
    createStatusBar();
    setupConnections();

    updateUIState();

    m_resizeDebounceTimer.setSingleShot(true);
    m_resizeDebounceTimer.setInterval(AppConfig::instance().resizeDebounceDelay());

    if (!DictionaryConnector::instance().isGoldenDictAvailable()) {
        qWarning() << "GoldenDict not found, lookup feature will not work";
    }
}

MainWindow::~MainWindow()
{
    while (m_tabWidget->count() > 0) {
        closeTab(0);
    }

    if (m_ocrInitialized) {
        OCRManager::instance().shutdown();
    }
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open PDF File"),
        QString(),
        tr("PDF Files (*.pdf);;All Files (*.*)")
        );

    if (filePath.isEmpty()) {
        return;
    }

    PDFDocumentTab* tab = currentTab();

    if (!tab || tab->isDocumentLoaded()) {
        tab = createNewTab();
    }

    QString errorMsg;
    if (!tab->loadDocument(filePath, &errorMsg)) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to open:\n%1\n\nError: %2")
                                  .arg(filePath).arg(errorMsg));

        if (m_tabWidget->count() > 1) {
            int index = m_tabWidget->indexOf(tab);
            closeTab(index);
        }
    }
}

void MainWindow::closeCurrentTab()
{
    int index = m_tabWidget->currentIndex();
    if (index >= 0) {
        closeTab(index);
    }
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabWidget->count()) {
        return;
    }

    PDFDocumentTab* tab = qobject_cast<PDFDocumentTab*>(m_tabWidget->widget(index));
    if (!tab) {
        return;
    }

    disconnectTabSignals(tab);

    if (tab == currentTab() && m_navigationDock) {
        m_navigationDock->setWidget(nullptr);
        m_navigationDock->setVisible(false);
    }

    m_tabWidget->removeTab(index);
    tab->deleteLater();

    updateUIState();
}

void MainWindow::quit()
{
    QApplication::quit();
}

PDFDocumentTab* MainWindow::currentTab() const
{
    return qobject_cast<PDFDocumentTab*>(m_tabWidget->currentWidget());
}

PDFDocumentTab* MainWindow::createNewTab()
{
    PDFDocumentTab* tab = new PDFDocumentTab(this);

    int index = m_tabWidget->addTab(tab, tr("New Tab"));
    m_tabWidget->setCurrentIndex(index);

    connectTabSignals(tab);

    return tab;
}

void MainWindow::connectTabSignals(PDFDocumentTab* tab)
{
    if (!tab) return;

    connect(tab, &PDFDocumentTab::documentLoaded,
            this, &MainWindow::onCurrentTabDocumentLoaded);

    connect(tab, &PDFDocumentTab::pageChanged,
            this, &MainWindow::onCurrentTabPageChanged);

    connect(tab, &PDFDocumentTab::zoomChanged,
            this, &MainWindow::onCurrentTabZoomChanged);

    connect(tab, &PDFDocumentTab::displayModeChanged,
            this, &MainWindow::onCurrentTabDisplayModeChanged);

    connect(tab, &PDFDocumentTab::continuousScrollChanged,
            this, &MainWindow::onCurrentTabContinuousScrollChanged);

    connect(tab, &PDFDocumentTab::textSelectionChanged,
            this, &MainWindow::onCurrentTabTextSelectionChanged);

    connect(tab, &PDFDocumentTab::searchCompleted,
            this, &MainWindow::onCurrentTabSearchCompleted);
}

void MainWindow::disconnectTabSignals(PDFDocumentTab* tab)
{
    if (!tab) return;
    disconnect(tab, nullptr, this, nullptr);
}

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index);

    PDFDocumentTab* tab = currentTab();

    if (tab && tab->isDocumentLoaded()) {
        if (tab->navigationPanel()) {
            m_navigationDock->setWidget(tab->navigationPanel());

            bool shouldShow = m_showNavigationAction->isChecked();
            m_navigationDock->setVisible(shouldShow);
            m_navPanelAction->setChecked(shouldShow);
        }

        bool canEnhance = !tab->isTextPDF();
        m_paperEffectAction->setEnabled(canEnhance);
        m_paperEffectAction->setChecked(canEnhance && tab->paperEffectEnabled());
        if (tab->isTextPDF()) {
            m_paperEffectAction->setToolTip(tr("Paper texture enhancement (scanned PDFs only)"));
        } else {
            m_paperEffectAction->setToolTip(tr("Paper texture enhancement"));
        }
    } else {
        m_navigationDock->setWidget(nullptr);
        m_navigationDock->setVisible(false);
        m_showNavigationAction->setChecked(false);
        m_navPanelAction->setChecked(false);
    }

    updateUIState();
    updateWindowTitle();
}

void MainWindow::onTabCloseRequested(int index)
{
    closeTab(index);
}

void MainWindow::updateTabTitle(int index)
{
    PDFDocumentTab* tab = qobject_cast<PDFDocumentTab*>(m_tabWidget->widget(index));
    if (tab) {
        QString fullTitle = tab->documentTitle();
        QString displayTitle = fullTitle;

        const int maxLength = 20;
        if (displayTitle.length() > maxLength) {
            QFileInfo fileInfo(fullTitle);
            QString baseName = fileInfo.completeBaseName();
            QString extension = fileInfo.suffix();

            if (!extension.isEmpty()) {
                int availableLength = maxLength - extension.length() - 4;

                if (baseName.length() > availableLength) {
                    baseName = baseName.left(availableLength);
                    displayTitle = baseName + "..." + "." + extension;
                } else {
                    displayTitle = fullTitle;
                }
            } else {
                displayTitle = displayTitle.left(maxLength - 3) + "...";
            }
        }

        m_tabWidget->setTabText(index, displayTitle);
        m_tabWidget->setTabToolTip(index, tab->documentPath());
    }
}

void MainWindow::previousPage()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->previousPage();
    }
}

void MainWindow::nextPage()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->nextPage();
    }
}

void MainWindow::firstPage()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->firstPage();
    }
}

void MainWindow::lastPage()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->lastPage();
    }
}

void MainWindow::goToPage(int page)
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->goToPage(page - 1);
    }
}

void MainWindow::zoomIn()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->zoomIn();
    }
}

void MainWindow::zoomOut()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->zoomOut();
    }
}

void MainWindow::actualSize()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->actualSize();
    }
}

void MainWindow::fitPage()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->fitPage();
    }
}

void MainWindow::fitWidth()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->fitWidth();
    }
}

void MainWindow::onZoomComboChanged(const QString& text)
{
    QString cleaned = text;
    cleaned.remove('%').remove(' ');
    bool ok;
    double zoom = cleaned.toDouble(&ok) / 100.0;

    if (ok && zoom > 0) {
        if (PDFDocumentTab* tab = currentTab()) {
            tab->setZoom(zoom);
        }
    }
}

void MainWindow::togglePageMode(PageDisplayMode mode)
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->setDisplayMode(mode);
    }
}

void MainWindow::toggleContinuousScroll()
{
    if (PDFDocumentTab* tab = currentTab()) {
        bool continuous = !tab->isContinuousScroll();
        tab->setContinuousScroll(continuous);
    }
}

void MainWindow::toggleNavigationPanel()
{
    PDFDocumentTab* tab = currentTab();
    if (!tab || !tab->isDocumentLoaded()) {
        return;
    }

    bool visible = !m_navigationDock->isVisible();

    if (visible && tab->navigationPanel()) {
        m_navigationDock->setWidget(tab->navigationPanel());
    }

    m_navigationDock->setVisible(visible);
    m_navPanelAction->setChecked(visible);
    m_showNavigationAction->setChecked(visible);

    QTimer::singleShot(0, this, [tab]() {
        ZoomMode mode = tab->zoomMode();
        if (mode == ZoomMode::FitWidth || mode == ZoomMode::FitPage) {
            QSize viewportSize = tab->getViewportSize();
            tab->updateZoom(viewportSize);
        }
    });
}

void MainWindow::toggleLinksVisible()
{
    bool visible = m_showLinksAction->isChecked();
    if (PDFDocumentTab* tab = currentTab()) {
        tab->setLinksVisible(visible);
    }
}

void MainWindow::showSearchBar()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->showSearchBar();
    }
}

void MainWindow::findNext()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->findNext();
    }
}

void MainWindow::findPrevious()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->findPrevious();
    }
}

void MainWindow::copySelectedText()
{
    if (PDFDocumentTab* tab = currentTab()) {
        tab->copySelectedText();
    }
}

void MainWindow::onCurrentTabPageChanged(int pageIndex)
{
    PDFDocumentTab* sender = qobject_cast<PDFDocumentTab*>(QObject::sender());
    if (sender != currentTab()) {
        return;
    }

    updateStatusBar();

    if (m_pageSpinBox) {
        m_pageSpinBox->blockSignals(true);
        m_pageSpinBox->setValue(pageIndex + 1);
        m_pageSpinBox->blockSignals(false);
    }

    updateUIState();
}

void MainWindow::onCurrentTabZoomChanged(double zoom)
{
    PDFDocumentTab* sender = qobject_cast<PDFDocumentTab*>(QObject::sender());
    if (sender != currentTab()) {
        return;
    }

    updateStatusBar();
    updateZoomCombox(zoom);
    updateUIState();
}

void MainWindow::updateZoomCombox(double zoom)
{
    if (m_zoomComboBox) {
        QString text = QString::number(qRound(zoom * 100)) + "%";
        int index = m_zoomComboBox->findText(text);

        m_zoomComboBox->blockSignals(true);
        if (index >= 0) {
            m_zoomComboBox->setCurrentIndex(index);
        } else {
            m_zoomComboBox->setEditText(text);
        }
        m_zoomComboBox->blockSignals(false);
    }
}

void MainWindow::onCurrentTabDisplayModeChanged(PageDisplayMode mode)
{
    PDFDocumentTab* sender = qobject_cast<PDFDocumentTab*>(QObject::sender());
    if (sender != currentTab()) {
        return;
    }

    updateUIState();
}

void MainWindow::onCurrentTabContinuousScrollChanged(bool continuous)
{
    PDFDocumentTab* sender = qobject_cast<PDFDocumentTab*>(QObject::sender());
    if (sender != currentTab()) {
        return;
    }

    updateUIState();
}

void MainWindow::onCurrentTabTextSelectionChanged()
{
    PDFDocumentTab* sender = qobject_cast<PDFDocumentTab*>(QObject::sender());
    if (sender != currentTab()) {
        return;
    }

    PDFDocumentTab* tab = currentTab();
    if (tab && m_copyAction) {
        m_copyAction->setEnabled(tab->hasTextSelection());
    }

    updateStatusBar();
}

void MainWindow::onCurrentTabDocumentLoaded(const QString& filePath, int pageCount)
{
    Q_UNUSED(filePath);
    Q_UNUSED(pageCount);

    PDFDocumentTab* tab = qobject_cast<PDFDocumentTab*>(QObject::sender());
    if (!tab) {
        return;
    }

    int index = m_tabWidget->indexOf(tab);
    if (index >= 0) {
        updateTabTitle(index);
    }

    if (tab == currentTab()) {
        updateWindowTitle();
        updateUIState();

        if (tab->isDocumentLoaded() && tab->navigationPanel()) {
            m_navigationDock->setWidget(tab->navigationPanel());

            m_navigationDock->setVisible(true);
            m_showNavigationAction->setChecked(true);
            m_navPanelAction->setChecked(true);
        }

        bool canEnhance = !tab->isTextPDF();
        m_paperEffectAction->setEnabled(canEnhance);

        if (tab->isTextPDF()) {
            m_paperEffectAction->setChecked(false);
        }

        tab->updateOCRHoverState();
    }

    updateUIState();
}

void MainWindow::onCurrentTabSearchCompleted(const QString& query, int totalMatches)
{
    Q_UNUSED(query);

    PDFDocumentTab* sender = qobject_cast<PDFDocumentTab*>(QObject::sender());
    if (sender != currentTab()) {
        return;
    }

    m_findNextAction->setEnabled(totalMatches > 0);
    m_findPreviousAction->setEnabled(totalMatches > 0);
}

void MainWindow::createActions()
{
    m_openAction = new QAction(QIcon(":icons/resources/icons/open-file.png"),
                               tr("Open"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setToolTip(tr("Open File (Ctrl+O)"));
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFile);

    m_closeAction = new QAction(QIcon(":icons/resources/icons/close-file.png"),
                               tr("Close"), this);
    m_closeAction->setShortcut(QKeySequence::Close);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    m_quitAction = new QAction(tr("Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::quit);

    m_copyAction = new QAction(tr("Copy"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_copyAction->setEnabled(false);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::copySelectedText);

    m_findAction = new QAction(QIcon(":icons/resources/icons/search.png"),
                               tr("Find"), this);
    m_findAction->setShortcut(QKeySequence::Find);
    m_findAction->setToolTip(tr("Search (Ctrl+F)"));
    connect(m_findAction, &QAction::triggered, this, &MainWindow::showSearchBar);

    m_findNextAction = new QAction(tr("Find Next"), this);
    m_findNextAction->setShortcut(QKeySequence::FindNext);
    m_findNextAction->setEnabled(false);
    connect(m_findNextAction, &QAction::triggered, this, &MainWindow::findNext);

    m_findPreviousAction = new QAction(tr("Find Previous"), this);
    m_findPreviousAction->setShortcut(QKeySequence::FindPrevious);
    m_findPreviousAction->setEnabled(false);
    connect(m_findPreviousAction, &QAction::triggered, this, &MainWindow::findPrevious);

    m_firstPageAction = new QAction(QIcon(":icons/resources/icons/first-arrow.png"),
                                    tr("First Page"), this);
    m_firstPageAction->setToolTip(tr("First Page (Home)"));
    connect(m_firstPageAction, &QAction::triggered, this, &MainWindow::firstPage);

    m_previousPageAction = new QAction(QIcon(":icons/resources/icons/left-arrow.png"),
                                       tr("Previous Page"), this);
    m_previousPageAction->setToolTip(tr("Previous Page (PgUp)"));
    connect(m_previousPageAction, &QAction::triggered, this, &MainWindow::previousPage);

    m_nextPageAction = new QAction(QIcon(":icons/resources/icons/right-arrow.png"),
                                   tr("Next Page"), this);
    m_nextPageAction->setToolTip(tr("Next Page (PgDown)"));
    connect(m_nextPageAction, &QAction::triggered, this, &MainWindow::nextPage);

    m_lastPageAction = new QAction(QIcon(":icons/resources/icons/last-arrow.png"),
                                   tr("Last Page"), this);
    m_lastPageAction->setToolTip(tr("Last Page (End)"));
    connect(m_lastPageAction, &QAction::triggered, this, &MainWindow::lastPage);

    m_zoomInAction = new QAction(QIcon(":icons/resources/icons/zoom-in.png"),
                                 tr("Zoom In"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    m_zoomInAction->setToolTip(tr("Zoom In (Ctrl++)"));
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

    m_zoomOutAction = new QAction(QIcon(":icons/resources/icons/zoom-out.png"),
                                  tr("Zoom Out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    m_zoomOutAction->setToolTip(tr("Zoom Out (Ctrl+-)"));
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

    m_fitPageAction = new QAction(QIcon(":icons/resources/icons/fit-to-page.png"),
                                  tr("Fit Page"), this);
    m_fitPageAction->setShortcut(tr("Ctrl+1"));
    m_fitPageAction->setToolTip(tr("Fit Page (Ctrl+1)"));
    m_fitPageAction->setCheckable(true);
    connect(m_fitPageAction, &QAction::triggered, this, &MainWindow::fitPage);

    m_fitWidthAction = new QAction(QIcon(":icons/resources/icons/fit-to-width.png"),
                                   tr("Fit Width"), this);
    m_fitWidthAction->setShortcut(tr("Ctrl+2"));
    m_fitWidthAction->setToolTip(tr("Fit Width (Ctrl+2)"));
    m_fitWidthAction->setCheckable(true);
    connect(m_fitWidthAction, &QAction::triggered, this, &MainWindow::fitWidth);

    m_pageModeGroup = new QActionGroup(this);
    m_pageModeGroup->setExclusive(true);

    m_singlePageAction = new QAction(QIcon(":icons/resources/icons/single-page-mode.png"),
                                     tr("Single Page"), this);
    m_singlePageAction->setCheckable(true);
    m_singlePageAction->setChecked(true);
    m_pageModeGroup->addAction(m_singlePageAction);
    connect(m_singlePageAction, &QAction::triggered, this, [this]() {
        togglePageMode(PageDisplayMode::SinglePage);
    });

    m_doublePageAction = new QAction(QIcon(":icons/resources/icons/double-page-mode.png"),
                                     tr("Double Page"), this);
    m_doublePageAction->setCheckable(true);
    m_pageModeGroup->addAction(m_doublePageAction);
    connect(m_doublePageAction, &QAction::triggered, this, [this]() {
        togglePageMode(PageDisplayMode::DoublePage);
    });

    m_continuousScrollAction = new QAction(QIcon(":icons/resources/icons/continuous-mode.png"),
                                           tr("Continuous Scroll"), this);
    m_continuousScrollAction->setCheckable(true);
    m_continuousScrollAction->setChecked(true);
    connect(m_continuousScrollAction, &QAction::triggered,
            this, &MainWindow::toggleContinuousScroll);

    m_navPanelAction = new QAction(QIcon(":icons/resources/icons/sidebar.png"),
                                   tr("Navigation Panel"), this);
    m_navPanelAction->setToolTip(tr("Show Navigation Panel (F9)"));
    m_navPanelAction->setCheckable(true);
    connect(m_navPanelAction, &QAction::triggered,
            this, &MainWindow::toggleNavigationPanel);

    m_showNavigationAction = m_navPanelAction;
    m_showNavigationAction->setShortcut(tr("F9"));

    m_showLinksAction = new QAction(tr("Show Link Borders"), this);
    m_showLinksAction->setCheckable(true);
    m_showLinksAction->setChecked(true);
    connect(m_showLinksAction, &QAction::triggered,
            this, &MainWindow::toggleLinksVisible);

    m_paperEffectAction = new QAction(QIcon(":icons/resources/icons/paper-effect.png"),
                                      tr("Paper Enhancement"), this);
    m_paperEffectAction->setToolTip(tr("Eye-protective paper texture enhancement"));
    m_paperEffectAction->setCheckable(true);
    m_paperEffectAction->setChecked(false);
    connect(m_paperEffectAction, &QAction::triggered,
            this, &MainWindow::togglePaperEffect);

    m_ocrHoverAction = new QAction(QIcon(":icons/resources/icons/ocr.png"),
                                   tr("OCR Lookup"), this);
    m_ocrHoverAction->setShortcut(QKeySequence(tr("Ctrl+Shift+O")));
    m_ocrHoverAction->setToolTip(tr("Enable OCR hover mode (Ctrl+Shift+O)\n"
                                    "Press Ctrl+Q to trigger recognition\n"
                                    "(Scanned PDFs only)"));
    m_ocrHoverAction->setCheckable(true);
    m_ocrHoverAction->setChecked(false);
    m_ocrHoverAction->setEnabled(false);
    connect(m_ocrHoverAction, &QAction::triggered,
            this, &MainWindow::toggleOCRHover);
}

void MainWindow::createMenuBar()
{
    menuBar()->setNativeMenuBar(false);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_closeAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_copyAction);
    editMenu->addSeparator();
    editMenu->addAction(m_findAction);
    editMenu->addAction(m_findNextAction);
    editMenu->addAction(m_findPreviousAction);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_fitPageAction);
    viewMenu->addAction(m_fitWidthAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_singlePageAction);
    viewMenu->addAction(m_doublePageAction);
    viewMenu->addAction(m_continuousScrollAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_showNavigationAction);
    viewMenu->addAction(m_showLinksAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_ocrHoverAction);
}

void MainWindow::createToolBar()
{
    m_toolBar = addToolBar(tr(""));
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(20, 20));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolBar->setContentsMargins(0, 0, 0, 0);
    m_toolBar->setObjectName("mainToolBar");

    m_toolBar->addAction(m_navPanelAction);
    m_toolBar->addSeparator();

    m_toolBar->addAction(m_openAction);
    m_toolBar->addSeparator();

    m_toolBar->addAction(m_firstPageAction);
    m_toolBar->addAction(m_previousPageAction);

    m_pageSpinBox = new QSpinBox(this);
    m_pageSpinBox->setMinimum(1);
    m_pageSpinBox->setMaximum(1);
    m_pageSpinBox->setEnabled(false);
    m_pageSpinBox->setAlignment(Qt::AlignCenter);
    m_pageSpinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_pageSpinBox->setObjectName("pageSpinBox");
    connect(m_pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::goToPage);
    m_toolBar->addWidget(m_pageSpinBox);

    m_toolBar->addAction(m_nextPageAction);
    m_toolBar->addAction(m_lastPageAction);
    m_toolBar->addSeparator();

    m_toolBar->addAction(m_zoomOutAction);

    m_zoomComboBox = new QComboBox(this);
    m_zoomComboBox->setEditable(true);
    m_zoomComboBox->setObjectName("zoomComboBox");
    m_zoomComboBox->addItems({
        "25%", "50%", "75%", "100%", "125%", "150%", "200%", "300%", "400%"
    });
    m_zoomComboBox->setCurrentText("100%");
    connect(m_zoomComboBox, &QComboBox::currentTextChanged,
            this, &MainWindow::onZoomComboChanged);
    m_toolBar->addWidget(m_zoomComboBox);

    m_toolBar->addAction(m_zoomInAction);
    m_toolBar->addSeparator();

    m_toolBar->addAction(m_fitPageAction);
    m_toolBar->addAction(m_fitWidthAction);
    m_toolBar->addSeparator();

    m_toolBar->addAction(m_singlePageAction);
    m_toolBar->addAction(m_doublePageAction);
    m_toolBar->addAction(m_continuousScrollAction);
    m_toolBar->addSeparator();

    m_toolBar->addAction(m_paperEffectAction);
    m_toolBar->addAction(m_ocrHoverAction);

    m_toolBar->addAction(m_findAction);
}

void MainWindow::createStatusBar()
{
    statusBar()->setObjectName("modernStatusBar");
    statusBar()->setSizeGripEnabled(true);

    m_statusLabel = new QLabel(tr(""));
    m_statusLabel->setObjectName("statusLabel");
    statusBar()->addWidget(m_statusLabel, 1);

    m_pageLabel = new QLabel();
    m_pageLabel->setObjectName("pageLabel");
    m_pageLabel->setMinimumWidth(120);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    statusBar()->addPermanentWidget(m_pageLabel);

    m_zoomLabel = new QLabel();
    m_zoomLabel->setObjectName("zoomLabel");
    m_zoomLabel->setMinimumWidth(100);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    statusBar()->addPermanentWidget(m_zoomLabel);

    m_ocrIndicator = new OCRStatusIndicator(this);
    statusBar()->addPermanentWidget(m_ocrIndicator);
    m_ocrIndicator->setState(OCREngineState::Uninitialized);

    connect(m_ocrIndicator, &OCRStatusIndicator::engineStartRequested,
            this, &MainWindow::initOCREngine);
    connect(m_ocrIndicator, &OCRStatusIndicator::engineStopRequested,
            this, &MainWindow::shutdownOCREngine);
    connect(&OCRManager::instance(), &OCRManager::engineStateChanged,
            this, &MainWindow::onOCREngineStateChanged);

    connect(&OCRManager::instance(), &OCRManager::ocrHoverEnabledChanged,
            this, &MainWindow::onOCRHoverEnabledChanged);

    updateStatusBar();
}

void MainWindow::setupConnections()
{
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::onTabChanged);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);

    connect(&m_resizeDebounceTimer, &QTimer::timeout, this, [this]() {
        PDFDocumentTab* tab = currentTab();
        if (tab && tab->isDocumentLoaded()) {
            ZoomMode mode = tab->zoomMode();
            if (mode == ZoomMode::FitWidth || mode == ZoomMode::FitPage) {
                QSize viewportSize = tab->getViewportSize();
                tab->updateZoom(viewportSize);
            }
        }
    });

    QShortcut* ocrShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q), this);
    connect(ocrShortcut, &QShortcut::activated, this, &MainWindow::triggerOCRAtCurrentPosition);
}

void MainWindow::triggerOCRAtCurrentPosition()
{
    if (!OCRManager::instance().isOCRHoverEnabled()) {
        qDebug() << "OCR hover not enabled globally";
        return;
    }

    PDFDocumentTab* tab = currentTab();
    if (!tab || !tab->isDocumentLoaded()) {
        qDebug() << "No document loaded";
        return;
    }

    tab->triggerOCRAtCurrentPosition();
}

void MainWindow::updateUIState()
{
    PDFDocumentTab* tab = currentTab();
    bool hasDocument = tab && tab->isDocumentLoaded();
    int pageCount = hasDocument ? tab->pageCount() : 0;
    int currentPage = hasDocument ? tab->currentPage() : 0;
    double zoom = hasDocument ? tab->zoom() : 1.0;
    bool continuousScroll = hasDocument ? tab->isContinuousScroll() : true;
    PageDisplayMode displayMode = hasDocument ? tab->displayMode() : PageDisplayMode::SinglePage;
    ZoomMode zoomMode = hasDocument ? tab->zoomMode() : ZoomMode::FitWidth;
    bool canEnhance = hasDocument && !tab->isTextPDF();

    m_closeAction->setEnabled(hasDocument);

    if (m_copyAction) {
        bool hasSelection = hasDocument && tab->hasTextSelection();
        m_copyAction->setEnabled(hasDocument && tab->isTextPDF() && hasSelection);
    }

    m_findAction->setEnabled(hasDocument && tab->isTextPDF());

    m_firstPageAction->setEnabled(hasDocument && currentPage > 0);
    m_previousPageAction->setEnabled(hasDocument && currentPage > 0);
    m_nextPageAction->setEnabled(hasDocument && currentPage < pageCount - 1);
    m_lastPageAction->setEnabled(hasDocument && currentPage < pageCount - 1);

    m_zoomInAction->setEnabled(hasDocument);
    m_zoomOutAction->setEnabled(hasDocument);
    m_fitPageAction->setEnabled(hasDocument);
    m_fitWidthAction->setEnabled(hasDocument);

    m_fitPageAction->setEnabled(hasDocument && zoomMode != ZoomMode::FitPage);
    m_fitPageAction->setChecked(hasDocument && zoomMode == ZoomMode::FitPage);
    m_fitWidthAction->setEnabled(hasDocument && zoomMode != ZoomMode::FitWidth);
    m_fitWidthAction->setChecked(hasDocument && zoomMode == ZoomMode::FitWidth);

    m_singlePageAction->setEnabled(hasDocument);
    m_doublePageAction->setEnabled(hasDocument);
    m_continuousScrollAction->setEnabled(hasDocument && displayMode == PageDisplayMode::SinglePage);

    m_singlePageAction->setChecked(hasDocument && displayMode == PageDisplayMode::SinglePage);
    m_doublePageAction->setChecked(hasDocument && displayMode == PageDisplayMode::DoublePage);
    m_continuousScrollAction->setChecked(hasDocument && continuousScroll);

    m_paperEffectAction->setEnabled(canEnhance);
    if (hasDocument && !canEnhance) {
        m_paperEffectAction->setToolTip(
            tr("Paper texture enhancement\n(Current is native text PDF, not applicable)"));
    } else if (canEnhance) {
        m_paperEffectAction->setToolTip(tr("Paper texture enhancement"));
    } else {
        m_paperEffectAction->setToolTip(tr("Paper texture enhancement (Open document first)"));
    }
    if (hasDocument) {
        m_paperEffectAction->setChecked(tab->paperEffectEnabled());

        if (tab->isTextPDF() && tab->paperEffectEnabled()) {
            tab->setPaperEffectEnabled(false);
        }
    }

    if (m_ocrHoverAction) {
        OCREngineState engineState = OCRManager::instance().engineState();
        bool ocrReady = (engineState == OCREngineState::Ready);

        QString tooltip;
        bool shouldEnable = false;

        if (!m_ocrInitialized) {
            tooltip = tr("Enable OCR hover (Ctrl+Shift+O)\n"
                         "Please start OCR engine in status bar first");
            shouldEnable = false;
        } else if (engineState == OCREngineState::Loading) {
            tooltip = tr("Enable OCR hover (Ctrl+Shift+O)\n"
                         "OCR engine loading, please wait...");
            shouldEnable = false;
        } else if (engineState == OCREngineState::Error) {
            tooltip = tr("Enable OCR hover (Ctrl+Shift+O)\n"
                         "OCR engine initialization failed");
            shouldEnable = false;
        } else if (ocrReady) {
            shouldEnable = true;
            if (!hasDocument) {
                tooltip = tr("Enable OCR hover (Ctrl+Shift+O)\n"
                             "Press Ctrl+Q to trigger\n"
                             "Open document first");
            } else if (tab->isTextPDF()) {
                tooltip = tr("Enable OCR hover (Ctrl+Shift+O)\n"
                             "Press Ctrl+Q to trigger\n"
                             "Current is text PDF, OCR not needed");
            } else {
                tooltip = tr("Enable OCR hover (Ctrl+Shift+O)\n"
                             "Press Ctrl+Q to trigger\n"
                             "Click to enable OCR hover feature");
            }
        }

        m_ocrHoverAction->setEnabled(shouldEnable);
        m_ocrHoverAction->setToolTip(tooltip);

        bool shouldCheck = ocrReady && OCRManager::instance().isOCRHoverEnabled();
        m_ocrHoverAction->setChecked(shouldCheck);
    }

    m_showNavigationAction->setEnabled(hasDocument);
    m_showLinksAction->setEnabled(hasDocument);

    m_navPanelAction->setEnabled(hasDocument);
    m_navPanelAction->setChecked(m_navigationDock->isVisible());

    if (m_pageSpinBox) {
        m_pageSpinBox->setEnabled(hasDocument);
        m_pageSpinBox->setMaximum(qMax(1, pageCount));
        if (hasDocument) {
            m_pageSpinBox->setValue(currentPage + 1);
            m_pageSpinBox->setSuffix(tr(" / %1").arg(pageCount));
        } else {
            m_pageSpinBox->setValue(1);
            m_pageSpinBox->setSuffix("");
        }
    }

    if (m_zoomComboBox) {
        m_zoomComboBox->setEnabled(hasDocument);
        updateZoomCombox(zoom);
    }

    updateStatusBar();
}

void MainWindow::updateWindowTitle()
{
    QString title = tr("MuQt");

    PDFDocumentTab* tab = currentTab();
    if (tab && tab->isDocumentLoaded()) {
        QString filePath = tab->documentPath();
        if (!filePath.isEmpty()) {
            QFileInfo fileInfo(filePath);
            title = fileInfo.fileName() + " - " + title;
        }
    }

    setWindowTitle(title);
}

void MainWindow::updateStatusBar()
{
    PDFDocumentTab* tab = currentTab();

    if (!tab || !tab->isDocumentLoaded()) {
        m_pageLabel->setText("");
        m_zoomLabel->setText("");
        m_statusLabel->setText(tr("Please open a PDF file"));
        return;
    }

    int currentPage = tab->currentPage() + 1;
    int pageCount = tab->pageCount();
    m_pageLabel->setText(tr("Page %1 / %2").arg(currentPage).arg(pageCount));

    double zoom = tab->zoom();
    QString zoomMode;
    switch (tab->zoomMode()) {
    case ZoomMode::FitPage:
        zoomMode = tr(" (Fit Page)");
        break;
    case ZoomMode::FitWidth:
        zoomMode = tr(" (Fit Width)");
        break;
    default:
        break;
    }
    m_zoomLabel->setText(tr("Zoom %1%%2").arg(qRound(zoom * 100)).arg(zoomMode));

    if (tab->hasTextSelection()) {
        m_statusLabel->setText(tr("Text selected"));
    } else {
        m_statusLabel->setText(tr(""));
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    m_resizeDebounceTimer.start();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    int tabCount = m_tabWidget->count();
    int loadedCount = 0;

    for (int i = 0; i < tabCount; ++i) {
        PDFDocumentTab* tab = qobject_cast<PDFDocumentTab*>(m_tabWidget->widget(i));
        if (tab && tab->isDocumentLoaded()) {
            loadedCount++;
        }
    }

    if (loadedCount > 1) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Close Application"),
            tr("You have %n document(s) open. Are you sure you want to close all of them?", "", loadedCount),
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::togglePaperEffect()
{
    PDFDocumentTab* tab = currentTab();
    if (!tab || !tab->isDocumentLoaded()) {
        return;
    }

    if (tab->isTextPDF()) {
        QMessageBox::information(this, tr("Feature Not Available"),
                                 tr("Paper enhancement is only for scanned PDFs.\n"
                                    "Current document is native text PDF and does not need this feature."));
        m_paperEffectAction->setChecked(false);
        return;
    }

    bool enabled = m_paperEffectAction->isChecked();
    tab->setPaperEffectEnabled(enabled);
}

QString MainWindow::getEngineStateText(OCREngineState state) const
{
    switch (state) {
    case OCREngineState::Uninitialized:
        return tr("Uninitialized");
    case OCREngineState::Loading:
        return tr("Loading");
    case OCREngineState::Ready:
        return tr("Ready");
    case OCREngineState::Error:
        return tr("Error");
    default:
        return tr("Unknown State");
    }
}

void MainWindow::toggleOCRHover()
{
    bool wantEnable = m_ocrHoverAction->isChecked();

    OCREngineState state = OCRManager::instance().engineState();

    if (wantEnable) {
        if (!m_ocrInitialized) {
            QMessageBox::information(this, tr("OCR Feature"),
                                     tr("Please start the OCR engine in the status bar first!\n\n"
                                        "Click the [OCR Engine] button on the right side of the status bar to start the engine."));
            m_ocrHoverAction->setChecked(false);
            return;
        }

        if (state == OCREngineState::Loading) {
            QMessageBox::information(this, tr("OCR Feature"),
                                     tr("OCR engine is loading...\n\n"
                                        "Please wait for the engine to finish loading (status indicator turns green) before enabling."));
            m_ocrHoverAction->setChecked(false);
            return;
        }

        if (state == OCREngineState::Error) {
            QMessageBox::warning(this, tr("OCR Feature"),
                                 tr("OCR engine initialization failed!\n\n"
                                    "Error message: %1\n\n"
                                    "Please try:\n"
                                    "1. Restart OCR engine\n"
                                    "2. Check model file integrity\n"
                                    "3. View logs for detailed error info")
                                     .arg(OCRManager::instance().lastError()));
            m_ocrHoverAction->setChecked(false);
            return;
        }

        if (state != OCREngineState::Ready) {
            QMessageBox::information(this, tr("OCR Feature"),
                                     tr("OCR engine not ready yet, cannot enable feature.\n\n"
                                        "Current state: %1")
                                         .arg(getEngineStateText(state)));
            m_ocrHoverAction->setChecked(false);
            return;
        }

        OCRManager::instance().setOCRHoverEnabled(true);

        QMessageBox::information(this, tr("OCR Hover Enabled"),
                                 tr("OCR hover lookup enabled!\n\n"
                                    "How to use:\n"
                                    "1. Move mouse to text position to recognize\n"
                                    "2. Press Ctrl+Q shortcut to trigger recognition\n"
                                    "3. Recognition result displays in popup\n"
                                    "4. Click popup to query dictionary\n"
                                    "5. Click toolbar button again to disable OCR\n\n"
                                    "Tip: View OCR engine status in status bar"));
    } else {
        OCRManager::instance().setOCRHoverEnabled(false);
    }
}

void MainWindow::initOCREngine()
{
    if (m_ocrInitialized) {
        qInfo() << "OCR engine already initialized";
        return;
    }

    QString modelDir = AppConfig::instance().ocrModelDir();
    QString dictDir = AppConfig::instance().jiebaDictDir();

    qInfo() << "MainWindow: Starting OCR engine...";
    qInfo() << "Model directory:" << modelDir;
    qInfo() << "Dictionary directory:" << dictDir;

    m_ocrIndicator->setEngineRunning(true);
    m_ocrIndicator->setState(OCREngineState::Loading);

    if (!ChineseTokenizer::instance().isInitialized()) {
        bool jiebaOk = ChineseTokenizer::instance().initialize(dictDir);
        if (!jiebaOk) {
            qWarning() << "Tokenizer initialization failed:"
                       << ChineseTokenizer::instance().lastError();
            QMessageBox::warning(this, tr("Tokenizer Initialization Failed"),
                                 tr("Chinese word segmentation initialization failed:\n%1\n\nOCR will use full text.")
                                     .arg(ChineseTokenizer::instance().lastError()));
        }
    }

    bool started = OCRManager::instance().initialize(modelDir);

    if (started) {
        m_ocrInitialized = true;
        qInfo() << "OCR engine starting...";

        statusBar()->showMessage(tr("OCR engine loading in background..."), 3000);
    } else {
        qWarning() << "OCR engine start failed";

        m_ocrIndicator->setEngineRunning(false);
        m_ocrIndicator->setState(OCREngineState::Error);

        QMessageBox::critical(this, tr("OCR Engine Start Failed"),
                              tr("Unable to start OCR engine, please check:\n"
                                 "1. Model files exist\n"
                                 "2. Model path configuration is correct\n"
                                 "3. System resources are sufficient\n\n"
                                 "Model directory: %1").arg(modelDir));

        m_ocrInitialized = false;
    }

    updateUIState();
}

void MainWindow::shutdownOCREngine()
{
    if (!m_ocrInitialized) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Stop OCR Engine"),
        tr("Are you sure you want to stop the OCR engine?\n\nOCR hover feature will be disabled."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        );

    if (reply != QMessageBox::Yes) {
        return;
    }

    if (OCRManager::instance().isOCRHoverEnabled()) {
        OCRManager::instance().setOCRHoverEnabled(false);
        if (m_ocrHoverAction) {
            m_ocrHoverAction->setChecked(false);
        }
    }

    OCRManager::instance().shutdown();
    m_ocrInitialized = false;

    m_ocrIndicator->setEngineRunning(false);
    m_ocrIndicator->setState(OCREngineState::Uninitialized);

    qInfo() << "OCR engine stopped";
    statusBar()->showMessage(tr("OCR engine stopped"), 2000);

    updateUIState();
}

void MainWindow::onOCREngineStateChanged(OCREngineState state)
{
    if (m_ocrIndicator) {
        m_ocrIndicator->setState(state);

        if (state == OCREngineState::Uninitialized) {
            m_ocrIndicator->setEngineRunning(false);
        } else {
            m_ocrIndicator->setEngineRunning(true);
        }
    }

    if (state == OCREngineState::Ready) {
        statusBar()->showMessage(tr("OCR engine ready, enable OCR hover in toolbar"), 3000);
    } else if (state == OCREngineState::Error) {
        statusBar()->showMessage(tr("OCR engine initialization failed"), 5000);
    }

    updateUIState();
}

void MainWindow::onOCRHoverEnabledChanged(bool enabled)
{
    updateUIState();

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        PDFDocumentTab* tab = qobject_cast<PDFDocumentTab*>(m_tabWidget->widget(i));
        if (tab && tab->isDocumentLoaded() && !tab->isTextPDF()) {
            tab->updateOCRHoverState();
        }
    }

    qInfo() << "OCR hover state changed to:" << enabled;
}
