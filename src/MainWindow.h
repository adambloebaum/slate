/**
 * Slate Browser - A Clean Slate for Web Browsing
 * MainWindow.h - Main browser window header
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QShortcut>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTabBar>
#include <QStackedWidget>
#include <QPoint>
#include <QSystemTrayIcon>
#include <QSet>
#include <QStringList>
#include <QMap>
#include <QIcon>
#include <QWebEngineFullScreenRequest>
#include "AddressBar.h"
#include "AdBlocker.h"

class QWebEngineView;
class QWebEngineProfile;
class QWebEngineDownloadRequest;
class ThemeToggle;
class QCompleter;
class QStringListModel;
class QProgressBar;
class QLabel;
class QLineEdit;
class QSplitter;

class SlateTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit SlateTabBar(QWidget *parent = nullptr);
    void setDarkMode(bool dark) { m_isDark = dark; update(); }
    void setAvailableWidth(int width) { m_availableWidth = width; updateGeometry(); update(); }
    void setTabFavicon(int index, const QIcon &icon);
    QIcon tabFavicon(int index) const;
protected:
    QSize tabSizeHint(int index) const override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
private:
    QRect closeButtonRect(int index) const;
    int calculateTabWidth() const;
    bool m_isDark = false;
    int m_availableWidth = 800;
    int m_lastHoverIndex = -1;
    QMap<int, QIcon> m_favicons;
    static constexpr int NORMAL_TAB_WIDTH = 200;
    static constexpr int MIN_TAB_WIDTH = 80;
    static constexpr int TABS_BEFORE_SHRINK = 5;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    // For frameless window dragging
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onNewTab();
    void onCloseTab(int index);
    void onTabChanged(int index);
    void onNavigate(const QString &url);
    void onUrlChanged(const QUrl &url);
    void onTitleChanged(const QString &title);
    void onBack();
    void onForward();
    void onReload();
    void onMinimize();
    void onMaximizeRestore();
    void onClose();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onThemeToggled(bool isDark);
    void onAddressFocusGained();
    void onAddressFocusLost();

    // Feature 1: Find in page
    void onFindInPage();
    void onFindNext();
    void onFindPrev();
    void onFindClose();
    void onFindTextChanged(const QString &text);

    // Feature 2: Zoom
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();

    // Feature 3: Loading progress
    void onLoadStarted();
    void onLoadProgress(int progress);
    void onLoadFinished(bool ok);

    // Feature 4: Favicon
    void onIconChanged(const QIcon &icon);

    // Feature 5: Fullscreen
    void onToggleFullscreen();
    void onFullScreenRequested(QWebEngineFullScreenRequest request);

    // Feature 9: Permission toast
    void showPermissionToast(const QString &message);

    // Feature 10: Downloads
    void onDownloadRequested(QWebEngineDownloadRequest *download);

    // Feature 11: Print
    void onPrint();

    // Feature 12: DevTools
    void onToggleDevTools();

    // Feature 13: About page
    void onAboutPage();

private:
    void setupUI();
    void setupFindBar();
    void setupShortcuts();
    void setupConnections();
    void setupTrayIcon();
    void initializeWebEngine();
    void loadStylesheet();
    void applyTheme(bool isDark);
    void updateNavigationButtons();
    void addToHistory(const QUrl &url);
    QString formatUrl(const QString &input);
    QWebEngineView* currentWebView() const;
    QWebEngineView* webViewAt(int index) const;

    // UI Components
    QWidget *m_centralWidget = nullptr;
    QVBoxLayout *m_mainLayout = nullptr;

    // Title bar (custom, for frameless window)
    QWidget *m_titleBar = nullptr;
    QHBoxLayout *m_titleBarLayout = nullptr;

    // Tab bar (in title bar area)
    SlateTabBar *m_tabBar = nullptr;
    QPushButton *m_newTabButton = nullptr;

    // Window controls
    QPushButton *m_minimizeButton = nullptr;
    QPushButton *m_maximizeButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    // Navigation bar (below tabs)
    QWidget *m_navBar = nullptr;
    QHBoxLayout *m_navLayout = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_forwardButton = nullptr;
    QPushButton *m_reloadButton = nullptr;
    AddressBar *m_addressBar = nullptr;

    // Content area
    QStackedWidget *m_stackedWidget = nullptr;
    QWidget *m_placeholderWidget = nullptr;

    // Find bar (Feature 1)
    QWidget *m_findBar = nullptr;
    QLineEdit *m_findEdit = nullptr;
    QPushButton *m_findCloseBtn = nullptr;
    QPushButton *m_findNextBtn = nullptr;
    QPushButton *m_findPrevBtn = nullptr;
    QLabel *m_findLabel = nullptr;

    // Loading indicator (Feature 3)
    QProgressBar *m_progressBar = nullptr;

    // Theme toggle
    ThemeToggle *m_themeToggle = nullptr;
    bool m_isDarkMode = false;

    // Fullscreen state (Feature 5)
    bool m_isFullscreen = false;

    // Permission toast (Feature 9)
    QLabel *m_toastLabel = nullptr;

    // Download bar (Feature 10)
    QWidget *m_downloadBar = nullptr;
    QLabel *m_downloadLabel = nullptr;
    QProgressBar *m_downloadProgress = nullptr;
    QPushButton *m_downloadCloseBtn = nullptr;

    // DevTools (Feature 12)
    QWebEngineView *m_devToolsView = nullptr;
    bool m_devToolsOpen = false;

    // System tray
    QSystemTrayIcon *m_trayIcon = nullptr;

    // Ad blocker
    AdBlocker *m_adBlocker = nullptr;
    QWebEngineProfile *m_profile = nullptr;
    bool m_webEngineInitialized = false;

    // In-session history for address bar suggestions
    QCompleter *m_urlCompleter = nullptr;
    QStringListModel *m_urlModel = nullptr;
    QStringList m_urlHistory;
    QSet<QString> m_urlHistorySet;

    // Shortcuts
    QShortcut *m_newTabShortcut = nullptr;
    QShortcut *m_closeTabShortcut = nullptr;
    QShortcut *m_reloadShortcut = nullptr;
    QShortcut *m_focusAddressShortcut = nullptr;
    QShortcut *m_nextTabShortcut = nullptr;
    QShortcut *m_prevTabShortcut = nullptr;
    QShortcut *m_findShortcut = nullptr;
    QShortcut *m_zoomInShortcut = nullptr;
    QShortcut *m_zoomInShortcut2 = nullptr;  // Ctrl+= alternative
    QShortcut *m_zoomOutShortcut = nullptr;
    QShortcut *m_zoomResetShortcut = nullptr;
    QShortcut *m_fullscreenShortcut = nullptr;
    QShortcut *m_escapeFullscreenShortcut = nullptr;
    QShortcut *m_printShortcut = nullptr;
    QShortcut *m_devToolsShortcut = nullptr;

    // For window dragging
    bool m_dragging = false;
    QPoint m_dragPosition;
};

#endif // MAINWINDOW_H
