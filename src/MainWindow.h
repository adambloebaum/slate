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
#include "AddressBar.h"
#include "AdBlocker.h"

class QWebEngineView;
class ThemeToggle;

class SlateTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit SlateTabBar(QWidget *parent = nullptr);
    void setDarkMode(bool dark) { m_isDark = dark; update(); }
    void setAvailableWidth(int width) { m_availableWidth = width; updateGeometry(); update(); }
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

private:
    void setupUI();
    void setupShortcuts();
    void setupConnections();
    void setupTrayIcon();
    void loadStylesheet();
    void applyTheme(bool isDark);
    void updateNavigationButtons();
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

    // Theme toggle
    ThemeToggle *m_themeToggle = nullptr;
    bool m_isDarkMode = false;

    // System tray
    QSystemTrayIcon *m_trayIcon = nullptr;

    // Ad blocker
    AdBlocker *m_adBlocker = nullptr;

    // Shortcuts
    QShortcut *m_newTabShortcut = nullptr;
    QShortcut *m_closeTabShortcut = nullptr;
    QShortcut *m_reloadShortcut = nullptr;
    QShortcut *m_focusAddressShortcut = nullptr;
    QShortcut *m_nextTabShortcut = nullptr;
    QShortcut *m_prevTabShortcut = nullptr;

    // For window dragging
    bool m_dragging = false;
    QPoint m_dragPosition;
};

#endif // MAINWINDOW_H
