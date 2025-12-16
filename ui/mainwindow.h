#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include "ocrengine.h"
#include "datastructure.h"

class QTabWidget;
class QToolBar;
class QDockWidget;
class QSpinBox;
class QComboBox;
class QLabel;
class QActionGroup;
class PDFDocumentTab;
class OCRStatusIndicator;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void openFile();
    void closeCurrentTab();
    void quit();

    void onTabChanged(int index);
    void onTabCloseRequested(int index);

    void previousPage();
    void nextPage();
    void firstPage();
    void lastPage();
    void goToPage(int page);

    void zoomIn();
    void zoomOut();
    void actualSize();
    void fitPage();
    void fitWidth();
    void onZoomComboChanged(const QString& text);

    void togglePageMode(PageDisplayMode mode);
    void toggleContinuousScroll();
    void toggleNavigationPanel();
    void toggleLinksVisible();

    void showSearchBar();
    void findNext();
    void findPrevious();

    void copySelectedText();

    void onCurrentTabPageChanged(int pageIndex);
    void onCurrentTabZoomChanged(double zoom);
    void onCurrentTabDisplayModeChanged(PageDisplayMode mode);
    void onCurrentTabContinuousScrollChanged(bool continuous);
    void onCurrentTabTextSelectionChanged();
    void onCurrentTabDocumentLoaded(const QString& filePath, int pageCount);
    void onCurrentTabSearchCompleted(const QString& query, int totalMatches);

    void togglePaperEffect();

    void toggleOCRHover();
    void onOCREngineStateChanged(OCREngineState state);
    void onOCRHoverEnabledChanged(bool enabled);
    void triggerOCRAtCurrentPosition();

    void toggleToolBar();

public slots:
    void openFileFromCommandLine(const QString& filePath);

private:
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void createActions();
    void setupConnections();

    void updateUIState();
    void updateWindowTitle();
    void updateStatusBar();
    void updateZoomCombox(double zoom);

    PDFDocumentTab* currentTab() const;
    PDFDocumentTab* createNewTab();
    void connectTabSignals(PDFDocumentTab* tab);
    void disconnectTabSignals(PDFDocumentTab* tab);
    void closeTab(int index);
    void updateTabTitle(int index);

    void initOCREngine();
    void shutdownOCREngine();
    QString getEngineStateText(OCREngineState state) const;

#ifdef Q_OS_WIN
private slots:
    void onManageFileAssociation();
public:
    void checkAndShowFirstRunDialog();
private:
    void handleFileAssociation(bool shouldRegister);
    QStringList getSupportedExtensions() const;
    QString getFileTypeName() const;
#endif

private:
    QTabWidget* m_tabWidget;
    QDockWidget* m_navigationDock;
    QToolBar* m_toolBar;
    QSpinBox* m_pageSpinBox;
    QComboBox* m_zoomComboBox;

    QLabel* m_statusLabel;
    QLabel* m_pageLabel;
    QLabel* m_zoomLabel;

    QAction* m_openAction;
    QAction* m_closeAction;
    QAction* m_quitAction;

    QAction* m_copyAction;
    QAction* m_findAction;
    QAction* m_findNextAction;
    QAction* m_findPreviousAction;

    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_fitPageAction;
    QAction* m_fitWidthAction;

    QActionGroup* m_pageModeGroup;
    QAction* m_singlePageAction;
    QAction* m_doublePageAction;
    QAction* m_continuousScrollAction;

    QAction* m_showNavigationAction;
    QAction* m_showLinksAction;

    QAction* m_firstPageAction;
    QAction* m_previousPageAction;
    QAction* m_nextPageAction;
    QAction* m_lastPageAction;

    QAction* m_navPanelAction;

    QAction* m_paperEffectAction;

    QTimer m_resizeDebounceTimer;

    QAction* m_ocrHoverAction;
    OCRStatusIndicator* m_ocrIndicator;
    bool m_ocrInitialized;

    QLabel* m_welcomeLabel;
    QAction* m_toggleToolBarAction;
};

#endif // MAINWINDOW_H
