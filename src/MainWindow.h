/**
 * Slate Browser
 * MainWindow.h - Browser window: chrome, tabs, and sessions
 *
 * Every tab opened by the user gets its own off-the-record
 * QWebEngineProfile (a "session"). Tabs opened by a page share their
 * opener's session. A session is destroyed when its last tab closes.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QWebEngineFullScreenRequest>

class AddressBar;
class Blocklist;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QWebEngineDownloadRequest;
class QWebEngineProfile;
class QWebEngineView;
class SlateTabBar;
class ThemeToggle;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    // Frameless window: drag from the title bar, resize from the edges
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void setupUI();
    void setupFindBar();
    void setupDownloadBar();
    void setupShortcuts();
    void setupConnections();
    void initializeWebEngine();

    // Sessions and tabs
    QWebEngineProfile *createSession();
    QWebEngineView *addTab(QWebEngineProfile *session);
    void closeTab(int index);
    void onNewTab();
    void onCloseTab(int index);
    void onTabChanged(int index);
    void onTabMoved(int from, int to);
    void onBurn();
    QWebEngineView *currentWebView() const;
    QWebEngineView *webViewAt(int index) const;

    // Navigation
    void onNavigate(const QString &input);
    void onUrlChanged(const QUrl &url);
    void onTitleChanged(const QString &title);
    void onIconChanged(const QIcon &icon);
    void onAddressFocusGained();
    void updateAddressBar();
    void updateNavigationButtons();
    void updateBlockedLabel();

    // Page features
    void onLoadStarted();
    void onLoadProgress(int progress);
    void onLoadFinished();
    void onFindInPage();
    void onFindClose();
    void findText(bool backward);
    void zoomBy(qreal delta);
    void setFullscreen(bool fullscreen);
    void onFullScreenRequested(QWebEngineFullScreenRequest request);
    void onDownloadRequested(QWebEngineDownloadRequest *download);
    void onPrint();
    void onToggleDevTools();
    void onAboutPage();

    // Window
    bool isZoomed() const;
    void setZoomed(bool zoomed);
    void applyTheme(bool isDark);
    void installScrollbarScript(QWebEngineProfile *session);
    QString internalPage(const QString &name) const;
    void showToast(const QString &message);

    // Title bar
    QWidget *m_titleBar = nullptr;
    SlateTabBar *m_tabBar = nullptr;
    QPushButton *m_newTabButton = nullptr;
    QPushButton *m_minimizeButton = nullptr;
    QPushButton *m_maximizeButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    // Navigation bar
    QWidget *m_navBar = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_forwardButton = nullptr;
    QPushButton *m_reloadButton = nullptr;
    AddressBar *m_addressBar = nullptr;
    QLabel *m_blockedLabel = nullptr;
    QPushButton *m_burnButton = nullptr;
    ThemeToggle *m_themeToggle = nullptr;
    QProgressBar *m_progressBar = nullptr;

    // Content: one QWebEngineView per tab, same order as the tab bar
    QStackedWidget *m_stackedWidget = nullptr;
    QWidget *m_placeholderWidget = nullptr;
    QLabel *m_toastLabel = nullptr;

    // Find bar
    QWidget *m_findBar = nullptr;
    QLineEdit *m_findEdit = nullptr;
    QLabel *m_findLabel = nullptr;
    QPushButton *m_findPrevBtn = nullptr;
    QPushButton *m_findNextBtn = nullptr;
    QPushButton *m_findCloseBtn = nullptr;

    // Download bar
    QWidget *m_downloadBar = nullptr;
    QLabel *m_downloadLabel = nullptr;
    QProgressBar *m_downloadProgress = nullptr;

    QPointer<QWebEngineView> m_devToolsView;
    Blocklist *m_blocklist = nullptr;

    bool m_isDarkMode = false;
    bool m_isFullscreen = false;
    bool m_dragging = false;
    QPoint m_dragPosition;
};

#endif // MAINWINDOW_H
