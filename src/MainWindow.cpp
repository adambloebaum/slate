/**
 * Slate Browser
 * MainWindow.cpp - Browser window: chrome, tabs, and sessions
 */

#include "MainWindow.h"
#include "AddressBar.h"
#include "Blocklist.h"
#include "RequestFilter.h"
#include "TabBar.h"
#include "UrlInput.h"
#include "Version.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineCertificateError>
#include <QWebEngineCookieStore>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFindTextResult>
#include <QWebEngineHistory>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineProfileBuilder>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

// Custom theme toggle — refined pill with smooth slide and subtle depth
class ThemeToggle : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)
public:
    explicit ThemeToggle(QWidget *parent = nullptr) : QWidget(parent), m_position(0), m_isDark(false)
    {
        setFixedSize(38, 20);
        setCursor(Qt::PointingHandCursor);
        m_animation = new QPropertyAnimation(this, "position", this);
        m_animation->setDuration(200);
        m_animation->setEasingCurve(QEasingCurve::InOutCubic);
    }

    bool isDark() const { return m_isDark; }
    qreal position() const { return m_position; }
    void setPosition(qreal pos) { m_position = pos; update(); }

    void setDark(bool dark) {
        if (m_isDark != dark) {
            m_isDark = dark;
            m_animation->setStartValue(m_position);
            m_animation->setEndValue(dark ? 1.0 : 0.0);
            m_animation->start();
            emit toggled(dark);
        }
    }

signals:
    void toggled(bool isDark);

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Track — subtle tinted background
        QColor trackColor = m_isDark ? QColor("#2e2e33") : QColor("#ddddd8");
        p.setBrush(trackColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1, height() - 1), height() / 2.0, height() / 2.0);

        // Thumb — crisp circle with subtle shadow effect via layering
        int thumbDiameter = height() - 6;
        qreal thumbX = 3 + m_position * (width() - thumbDiameter - 6);
        qreal thumbY = 3;

        // Shadow layer (subtle depth)
        QColor shadowColor = m_isDark ? QColor(0, 0, 0, 50) : QColor(0, 0, 0, 25);
        p.setBrush(shadowColor);
        p.drawEllipse(QRectF(thumbX, thumbY + 0.5, thumbDiameter, thumbDiameter));

        // Main thumb
        QColor thumbColor = m_isDark ? QColor("#8a8a90") : QColor("#ffffff");
        p.setBrush(thumbColor);
        p.drawEllipse(QRectF(thumbX, thumbY, thumbDiameter, thumbDiameter));
    }

    void mousePressEvent(QMouseEvent *) override {
        setDark(!m_isDark);
    }

private:
    qreal m_position;
    bool m_isDark;
    QPropertyAnimation *m_animation;
};

// Include MOC for ThemeToggle
#include "MainWindow.moc"

static QString readResource(const QString &path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        qFatal("Missing resource %s", qPrintable(path));
    }
    return QString::fromUtf8(file.readAll());
}

// Welcome, about, and blank pages are loaded with setHtml and have no real URL
static bool isInternalPage(const QUrl &url)
{
    return url.isEmpty() || url.scheme() == "about" || url.scheme() == "data";
}

// Generate JavaScript that injects scrollbar CSS into the page.
// Uses solid mid-tone colors so the thumb is visible on any page background.
static QString scrollbarInjectionScript(bool isDark)
{
    // Solid grays that contrast against both light and dark page backgrounds
    QString thumbColor = isDark ? "#5a5a62" : "#c2c1bd";
    QString thumbHover = isDark ? "#75757d" : "#a5a4a0";
    QString trackColor = isDark ? "#26262b" : "#f2f2f0";

    return QString(R"(
        (function() {
            var id = '__slate_scrollbar';
            var el = document.getElementById(id);
            if (!el) {
                el = document.createElement('style');
                el.id = id;
                (document.head || document.documentElement).appendChild(el);
            }
            el.textContent = '\
                ::-webkit-scrollbar { width: 14px !important; height: 14px !important; } \
                ::-webkit-scrollbar-track { background: %3 !important; } \
                ::-webkit-scrollbar-thumb { \
                    background: %1 !important; \
                    border-radius: 7px !important; \
                    border: 3px solid %3 !important; \
                    background-clip: content-box !important; \
                } \
                ::-webkit-scrollbar-thumb:hover { \
                    background: %2 !important; \
                    background-clip: content-box !important; \
                } \
                ::-webkit-scrollbar-corner { background: %3 !important; } \
            ';
        })();
    )").arg(thumbColor, thumbHover, trackColor);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    setupUI();
    setupShortcuts();
    setupConnections();

    // Default to dark mode on launch
    m_themeToggle->setDark(true);

    setWindowTitle("Slate");
    resize(1200, 800);
    setMinimumSize(800, 600);

    showMaximized();
    m_maximizeButton->setText("❐");

    // Defer WebEngine initialization to speed up window appearance
    QTimer::singleShot(0, this, &MainWindow::initializeWebEngine);
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === TITLE BAR (tabs + window controls) ===
    m_titleBar = new QWidget();
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(48);

    QHBoxLayout *titleBarLayout = new QHBoxLayout(m_titleBar);
    titleBarLayout->setContentsMargins(8, 0, 0, 0);
    titleBarLayout->setSpacing(2);

    m_tabBar = new SlateTabBar();

    m_newTabButton = new QPushButton("+");
    m_newTabButton->setObjectName("newTabButton");
    m_newTabButton->setFixedSize(26, 26);
    m_newTabButton->setToolTip("New Tab (Ctrl+T)");
    m_newTabButton->setCursor(Qt::PointingHandCursor);

    // Spacer to push window controls to right
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_minimizeButton = new QPushButton("─");
    m_minimizeButton->setObjectName("windowButton");
    m_minimizeButton->setFixedSize(46, 48);
    m_minimizeButton->setToolTip("Minimize");
    m_minimizeButton->setCursor(Qt::PointingHandCursor);

    m_maximizeButton = new QPushButton("□");
    m_maximizeButton->setObjectName("windowButton");
    m_maximizeButton->setFixedSize(46, 48);
    m_maximizeButton->setToolTip("Maximize");
    m_maximizeButton->setCursor(Qt::PointingHandCursor);

    m_closeButton = new QPushButton("✕");
    m_closeButton->setObjectName("closeButton");
    m_closeButton->setFixedSize(46, 48);
    m_closeButton->setToolTip("Close");
    m_closeButton->setCursor(Qt::PointingHandCursor);

    titleBarLayout->addWidget(m_tabBar);
    titleBarLayout->addWidget(m_newTabButton);
    titleBarLayout->addWidget(spacer);
    titleBarLayout->addWidget(m_minimizeButton);
    titleBarLayout->addWidget(m_maximizeButton);
    titleBarLayout->addWidget(m_closeButton);

    // === NAVIGATION BAR ===
    m_navBar = new QWidget();
    m_navBar->setObjectName("navBar");
    m_navBar->setFixedHeight(46);

    QHBoxLayout *navLayout = new QHBoxLayout(m_navBar);
    navLayout->setContentsMargins(12, 4, 12, 8);
    navLayout->setSpacing(4);

    m_backButton = new QPushButton("<");
    m_backButton->setObjectName("navButton");
    m_backButton->setFixedSize(34, 34);
    m_backButton->setEnabled(false);
    m_backButton->setToolTip("Back (Alt+Left)");
    m_backButton->setCursor(Qt::PointingHandCursor);

    m_forwardButton = new QPushButton(">");
    m_forwardButton->setObjectName("navButton");
    m_forwardButton->setFixedSize(34, 34);
    m_forwardButton->setEnabled(false);
    m_forwardButton->setToolTip("Forward (Alt+Right)");
    m_forwardButton->setCursor(Qt::PointingHandCursor);

    m_reloadButton = new QPushButton("↻");
    m_reloadButton->setObjectName("navButton");
    m_reloadButton->setFixedSize(34, 34);
    m_reloadButton->setToolTip("Reload (Ctrl+R)");
    m_reloadButton->setCursor(Qt::PointingHandCursor);

    m_addressBar = new AddressBar();

    m_blockedLabel = new QLabel();
    m_blockedLabel->setObjectName("blockedLabel");
    m_blockedLabel->setToolTip("Ad and tracker requests blocked in this tab's session");

    m_burnButton = new QPushButton("burn");
    m_burnButton->setObjectName("burnButton");
    m_burnButton->setFixedHeight(26);
    m_burnButton->setToolTip("Close every tab and destroy every session (Ctrl+Shift+Del)");
    m_burnButton->setCursor(Qt::PointingHandCursor);

    m_themeToggle = new ThemeToggle();

    navLayout->addWidget(m_backButton);
    navLayout->addWidget(m_forwardButton);
    navLayout->addWidget(m_reloadButton);
    navLayout->addSpacing(8);
    navLayout->addWidget(m_addressBar, 1);
    navLayout->addSpacing(8);
    navLayout->addWidget(m_blockedLabel);
    navLayout->addWidget(m_burnButton);
    navLayout->addSpacing(8);
    navLayout->addWidget(m_themeToggle);

    // === LOADING BAR ===
    m_progressBar = new QProgressBar();
    m_progressBar->setObjectName("loadingBar");
    m_progressBar->setFixedHeight(2);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->hide();

    // === CONTENT AREA ===
    m_stackedWidget = new QStackedWidget();
    m_stackedWidget->setObjectName("contentArea");

    // Placeholder while WebEngine initializes
    m_placeholderWidget = new QWidget();
    QVBoxLayout *placeholderLayout = new QVBoxLayout(m_placeholderWidget);
    placeholderLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *placeholderLabel = new QLabel("Starting Slate...");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLayout->addWidget(placeholderLabel);
    m_stackedWidget->addWidget(m_placeholderWidget);

    // Toast shown over the top of the page (blocked permissions, burn)
    m_toastLabel = new QLabel(m_stackedWidget);
    m_toastLabel->setObjectName("permissionToast");
    m_toastLabel->setFixedHeight(32);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->hide();

    setupFindBar();
    setupDownloadBar();

    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_navBar);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_stackedWidget, 1);
    mainLayout->addWidget(m_findBar);
    mainLayout->addWidget(m_downloadBar);
}

void MainWindow::setupFindBar()
{
    m_findBar = new QWidget();
    m_findBar->setObjectName("findBar");
    m_findBar->setFixedHeight(40);
    m_findBar->hide();

    QHBoxLayout *findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(12, 4, 12, 4);
    findLayout->setSpacing(4);

    m_findEdit = new QLineEdit();
    m_findEdit->setObjectName("findEdit");
    m_findEdit->setPlaceholderText("Find in page...");
    m_findEdit->setFixedWidth(260);
    m_findEdit->setFixedHeight(28);

    m_findLabel = new QLabel();
    m_findLabel->setObjectName("findLabel");

    m_findPrevBtn = new QPushButton("▲");
    m_findPrevBtn->setObjectName("findButton");
    m_findPrevBtn->setFixedSize(28, 28);
    m_findPrevBtn->setToolTip("Previous match");
    m_findPrevBtn->setCursor(Qt::PointingHandCursor);

    m_findNextBtn = new QPushButton("▼");
    m_findNextBtn->setObjectName("findButton");
    m_findNextBtn->setFixedSize(28, 28);
    m_findNextBtn->setToolTip("Next match");
    m_findNextBtn->setCursor(Qt::PointingHandCursor);

    m_findCloseBtn = new QPushButton("✕");
    m_findCloseBtn->setObjectName("findCloseButton");
    m_findCloseBtn->setFixedSize(28, 28);
    m_findCloseBtn->setToolTip("Close (Esc)");
    m_findCloseBtn->setCursor(Qt::PointingHandCursor);

    findLayout->addWidget(m_findEdit);
    findLayout->addWidget(m_findLabel);
    findLayout->addWidget(m_findPrevBtn);
    findLayout->addWidget(m_findNextBtn);
    findLayout->addStretch();
    findLayout->addWidget(m_findCloseBtn);
}

void MainWindow::setupDownloadBar()
{
    m_downloadBar = new QWidget();
    m_downloadBar->setObjectName("downloadBar");
    m_downloadBar->setFixedHeight(40);
    m_downloadBar->hide();

    QHBoxLayout *layout = new QHBoxLayout(m_downloadBar);
    layout->setContentsMargins(12, 4, 12, 4);
    layout->setSpacing(8);

    m_downloadLabel = new QLabel();
    m_downloadLabel->setObjectName("downloadLabel");

    m_downloadProgress = new QProgressBar();
    m_downloadProgress->setObjectName("downloadProgress");
    m_downloadProgress->setFixedWidth(180);
    m_downloadProgress->setFixedHeight(4);
    m_downloadProgress->setTextVisible(false);
    m_downloadProgress->setRange(0, 100);

    QPushButton *closeButton = new QPushButton("✕");
    closeButton->setObjectName("findCloseButton");
    closeButton->setFixedSize(28, 28);
    closeButton->setCursor(Qt::PointingHandCursor);
    connect(closeButton, &QPushButton::clicked, m_downloadBar, &QWidget::hide);

    layout->addWidget(m_downloadLabel);
    layout->addWidget(m_downloadProgress);
    layout->addStretch();
    layout->addWidget(closeButton);
}

void MainWindow::setupShortcuts()
{
    auto bind = [this](const char *keys, auto handler) {
        connect(new QShortcut(QKeySequence(keys), this), &QShortcut::activated, this, handler);
    };

    bind("Ctrl+T", &MainWindow::onNewTab);
    bind("Ctrl+W", [this]() { onCloseTab(m_tabBar->currentIndex()); });
    bind("Ctrl+Shift+Del", &MainWindow::onBurn);
    bind("Ctrl+Tab", [this]() {
        m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + 1) % m_tabBar->count());
    });
    bind("Ctrl+Shift+Tab", [this]() {
        m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + m_tabBar->count() - 1) % m_tabBar->count());
    });

    bind("Ctrl+L", [this]() {
        m_addressBar->setFocus();
        m_addressBar->selectAll();
    });
    bind("Ctrl+R", [this]() { m_reloadButton->click(); });
    bind("F5", [this]() { m_reloadButton->click(); });
    bind("Alt+Left", [this]() { m_backButton->click(); });
    bind("Alt+Right", [this]() { m_forwardButton->click(); });

    bind("Ctrl+F", &MainWindow::onFindInPage);
    bind("Ctrl++", [this]() { zoomBy(0.1); });
    bind("Ctrl+=", [this]() { zoomBy(0.1); });
    bind("Ctrl+-", [this]() { zoomBy(-0.1); });
    bind("Ctrl+0", [this]() {
        if (auto *view = currentWebView()) view->setZoomFactor(1.0);
    });

    bind("F11", [this]() { setFullscreen(!m_isFullscreen); });
    bind("Escape", [this]() {
        if (m_isFullscreen) setFullscreen(false);
        else if (m_findBar->isVisible()) onFindClose();
    });

    bind("Ctrl+P", &MainWindow::onPrint);
    bind("F12", &MainWindow::onToggleDevTools);
    bind("F1", &MainWindow::onAboutPage);
}

void MainWindow::setupConnections()
{
    // Navigation buttons
    connect(m_backButton, &QPushButton::clicked, this, [this]() {
        if (auto *view = currentWebView()) view->back();
    });
    connect(m_forwardButton, &QPushButton::clicked, this, [this]() {
        if (auto *view = currentWebView()) view->forward();
    });
    connect(m_reloadButton, &QPushButton::clicked, this, [this]() {
        if (auto *view = currentWebView()) view->reload();
    });
    connect(m_newTabButton, &QPushButton::clicked, this, &MainWindow::onNewTab);
    connect(m_burnButton, &QPushButton::clicked, this, &MainWindow::onBurn);

    // Window controls
    connect(m_minimizeButton, &QPushButton::clicked, this, &MainWindow::showMinimized);
    connect(m_maximizeButton, &QPushButton::clicked, this, [this]() { setZoomed(!isZoomed()); });
    connect(m_closeButton, &QPushButton::clicked, this, &MainWindow::close);

    // Address bar
    connect(m_addressBar, &AddressBar::urlEntered, this, &MainWindow::onNavigate);
    connect(m_addressBar, &AddressBar::focusGained, this, &MainWindow::onAddressFocusGained);
    connect(m_addressBar, &AddressBar::focusLost, this, &MainWindow::updateAddressBar);

    // Tab bar
    connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::onCloseTab);
    connect(m_tabBar, &QTabBar::tabMoved, this, &MainWindow::onTabMoved);

    connect(m_themeToggle, &ThemeToggle::toggled, this, &MainWindow::applyTheme);

    // Find bar
    connect(m_findEdit, &QLineEdit::textChanged, this, [this]() { findText(false); });
    connect(m_findEdit, &QLineEdit::returnPressed, this, [this]() { findText(false); });
    connect(m_findNextBtn, &QPushButton::clicked, this, [this]() { findText(false); });
    connect(m_findPrevBtn, &QPushButton::clicked, this, [this]() { findText(true); });
    connect(m_findCloseBtn, &QPushButton::clicked, this, &MainWindow::onFindClose);
}

void MainWindow::initializeWebEngine()
{
    m_blocklist = new Blocklist(this);
    m_blocklist->loadHostsFile(QCoreApplication::applicationDirPath() + "/blocklist.txt");
    m_blocklist->syncRemoteList();

    onNewTab();
}

// === Sessions and tabs ===

QWebEngineProfile *MainWindow::createSession()
{
    // Off-the-record: cookies, storage, and cache live in memory only
    QWebEngineProfile *session = QWebEngineProfileBuilder::createOffTheRecordProfile(this);
    session->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
    session->cookieStore()->setCookieFilter([](const QWebEngineCookieStore::FilterRequest &request) {
        return !request.thirdParty;
    });

    QWebEngineSettings *settings = session->settings();
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    // Do not expose local network addresses through WebRTC
    settings->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, true);

    RequestFilter *filter = new RequestFilter(m_blocklist, session);
    session->setUrlRequestInterceptor(filter);
    connect(filter, &RequestFilter::blockedCountChanged, this, &MainWindow::updateBlockedLabel);

    connect(session, &QWebEngineProfile::downloadRequested, this, &MainWindow::onDownloadRequested);
    installScrollbarScript(session);
    return session;
}

QWebEngineView *MainWindow::addTab(QWebEngineProfile *session)
{
    QWebEngineView *view = new QWebEngineView();
    QWebEnginePage *page = new QWebEnginePage(session, view);
    view->setPage(page);

    // Set background color to match theme before loading (prevents white flash)
    page->setBackgroundColor(m_isDarkMode ? QColor("#161619") : QColor("#fafaf9"));

    connect(view, &QWebEngineView::urlChanged, this, &MainWindow::onUrlChanged);
    connect(view, &QWebEngineView::titleChanged, this, &MainWindow::onTitleChanged);
    connect(view, &QWebEngineView::iconChanged, this, &MainWindow::onIconChanged);
    connect(view, &QWebEngineView::loadStarted, this, &MainWindow::onLoadStarted);
    connect(view, &QWebEngineView::loadProgress, this, &MainWindow::onLoadProgress);
    connect(view, &QWebEngineView::loadFinished, this, &MainWindow::onLoadFinished);
    connect(page, &QWebEnginePage::fullScreenRequested, this, &MainWindow::onFullScreenRequested);

    // Deny every permission request, and say so
    connect(page, &QWebEnginePage::permissionRequested, this, [this](QWebEnginePermission permission) {
        permission.deny();

        QString name;
        switch (permission.permissionType()) {
            case QWebEnginePermission::PermissionType::MediaAudioCapture: name = "Microphone"; break;
            case QWebEnginePermission::PermissionType::MediaVideoCapture: name = "Camera"; break;
            case QWebEnginePermission::PermissionType::MediaAudioVideoCapture: name = "Camera & Microphone"; break;
            case QWebEnginePermission::PermissionType::Geolocation: name = "Location"; break;
            case QWebEnginePermission::PermissionType::DesktopVideoCapture:
            case QWebEnginePermission::PermissionType::DesktopAudioVideoCapture: name = "Screen sharing"; break;
            case QWebEnginePermission::PermissionType::Notifications: name = "Notifications"; break;
            case QWebEnginePermission::PermissionType::ClipboardReadWrite: name = "Clipboard"; break;
            default: name = "Permission"; break;
        }
        showToast(name + " blocked for " + permission.origin().host());
    });

    // Untrusted certificate: refuse the connection and explain why
    connect(page, &QWebEnginePage::certificateError, this, [this, view](QWebEngineCertificateError error) {
        error.rejectCertificate();
        view->setHtml(internalPage("cert-error").replace("{{host}}", error.url().host().toHtmlEscaped()),
                      error.url());
    });

    // target="_blank" and window.open: new tab in the opener's session,
    // so sign-in popups can talk back to the page that opened them
    connect(page, &QWebEnginePage::newWindowRequested, this, [this, session](QWebEngineNewWindowRequest &request) {
        request.openIn(addTab(session)->page());
    });

    connect(page, &QWebEnginePage::findTextFinished, this, [this, view](const QWebEngineFindTextResult &result) {
        if (view != currentWebView()) return;
        if (m_findEdit->text().isEmpty()) {
            m_findLabel->clear();
        } else {
            m_findLabel->setText(QString("%1 of %2").arg(result.activeMatch()).arg(result.numberOfMatches()));
        }
    });

    m_stackedWidget->addWidget(view);
    if (m_placeholderWidget) {
        m_stackedWidget->removeWidget(m_placeholderWidget);
        delete m_placeholderWidget;
        m_placeholderWidget = nullptr;
    }
    m_tabBar->setCurrentIndex(m_tabBar->addTab("New Tab"));
    return view;
}

void MainWindow::onNewTab()
{
    addTab(createSession())->setHtml(internalPage("welcome"));
    m_addressBar->clear();
    m_addressBar->setFocus();
}

void MainWindow::closeTab(int index)
{
    QWebEngineView *view = webViewAt(index);
    QWebEngineProfile *session = view->page()->profile();

    if (m_devToolsView && m_devToolsView->page()->inspectedPage() == view->page()) {
        delete m_devToolsView;
    }

    m_stackedWidget->removeWidget(view);
    m_tabBar->removeTab(index);
    delete view;

    // A session dies with its last tab, taking its cookies and storage with it
    for (int i = 0; i < m_stackedWidget->count(); ++i) {
        if (webViewAt(i)->page()->profile() == session) {
            return;
        }
    }
    delete session;
}

void MainWindow::onCloseTab(int index)
{
    if (m_tabBar->count() == 1) {
        close();
        return;
    }
    closeTab(index);
    onTabChanged(m_tabBar->currentIndex());
}

void MainWindow::onBurn()
{
    while (m_tabBar->count() > 0) {
        closeTab(0);
    }
    m_findBar->hide();
    m_downloadBar->hide();
    m_progressBar->hide();

    onNewTab();
    showToast("Burned. Every tab and session is gone.");
}

void MainWindow::onTabMoved(int from, int to)
{
    // Sync QStackedWidget order with tab bar after drag-reorder
    QWidget *widget = m_stackedWidget->widget(from);
    m_stackedWidget->removeWidget(widget);
    m_stackedWidget->insertWidget(to, widget);
    m_stackedWidget->setCurrentIndex(m_tabBar->currentIndex());
}

void MainWindow::onTabChanged(int index)
{
    if (index < 0 || index >= m_stackedWidget->count()) {
        return;
    }
    m_stackedWidget->setCurrentIndex(index);
    m_progressBar->hide();
    updateAddressBar();
    updateNavigationButtons();
    updateBlockedLabel();
}

QWebEngineView *MainWindow::currentWebView() const
{
    return qobject_cast<QWebEngineView*>(m_stackedWidget->currentWidget());
}

QWebEngineView *MainWindow::webViewAt(int index) const
{
    return qobject_cast<QWebEngineView*>(m_stackedWidget->widget(index));
}

// === Navigation ===

void MainWindow::onNavigate(const QString &input)
{
    if (auto *view = currentWebView()) {
        view->load(urlFromInput(input));
        view->setFocus();
    }
}

void MainWindow::onUrlChanged(const QUrl &)
{
    if (sender() == currentWebView()) {
        updateAddressBar();
        updateNavigationButtons();
    }
}

void MainWindow::onTitleChanged(const QString &title)
{
    int index = m_stackedWidget->indexOf(qobject_cast<QWidget*>(sender()));
    if (index >= 0 && index < m_tabBar->count()) {
        // Pages without a title report their URL; do not show that for internal pages
        bool untitled = title.isEmpty() || title.startsWith("about:") || title.startsWith("data:");
        m_tabBar->setTabText(index, untitled ? "New Tab" : title);
    }
}

void MainWindow::onIconChanged(const QIcon &icon)
{
    int index = m_stackedWidget->indexOf(qobject_cast<QWidget*>(sender()));
    if (index >= 0 && index < m_tabBar->count()) {
        m_tabBar->setTabIcon(index, icon);
    }
}

void MainWindow::onAddressFocusGained()
{
    // Show the full URL while editing
    if (auto *view = currentWebView()) {
        if (!isInternalPage(view->url())) {
            m_addressBar->setText(view->url().toString());
        }
    }
}

void MainWindow::updateAddressBar()
{
    // Show just the host for a cleaner look
    if (auto *view = currentWebView()) {
        const QUrl url = view->url();
        if (isInternalPage(url)) {
            m_addressBar->clear();
        } else {
            m_addressBar->setText(url.host().isEmpty() ? url.toString() : url.host());
        }
    }
}

void MainWindow::updateNavigationButtons()
{
    if (auto *view = currentWebView()) {
        m_backButton->setEnabled(view->history()->canGoBack());
        m_forwardButton->setEnabled(view->history()->canGoForward());
    }
}

void MainWindow::updateBlockedLabel()
{
    int blocked = 0;
    if (auto *view = currentWebView()) {
        blocked = view->page()->profile()->findChild<RequestFilter*>()->blockedCount();
    }
    m_blockedLabel->setText(blocked > 0 ? QString("%1 blocked").arg(blocked) : QString());
}

// === Loading progress ===

void MainWindow::onLoadStarted()
{
    if (sender() == currentWebView()) {
        m_progressBar->setValue(0);
        m_progressBar->show();
    }
}

void MainWindow::onLoadProgress(int progress)
{
    if (sender() == currentWebView()) {
        m_progressBar->setValue(progress);
    }
}

void MainWindow::onLoadFinished()
{
    if (sender() == currentWebView()) {
        m_progressBar->hide();
    }
}

// === Find in page ===

void MainWindow::onFindInPage()
{
    if (m_findBar->isVisible()) {
        onFindClose();
        return;
    }
    m_findBar->show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void MainWindow::onFindClose()
{
    m_findBar->hide();
    if (auto *view = currentWebView()) {
        view->findText(QString());  // Clear highlights
    }
}

void MainWindow::findText(bool backward)
{
    if (auto *view = currentWebView()) {
        view->findText(m_findEdit->text(),
                       backward ? QWebEnginePage::FindBackward : QWebEnginePage::FindFlags());
    }
}

// === Zoom ===

void MainWindow::zoomBy(qreal delta)
{
    if (auto *view = currentWebView()) {
        view->setZoomFactor(qBound(0.25, view->zoomFactor() + delta, 5.0));
    }
}

// === Fullscreen ===

void MainWindow::setFullscreen(bool fullscreen)
{
    if (m_isFullscreen == fullscreen) {
        return;
    }
    m_isFullscreen = fullscreen;

    m_titleBar->setVisible(!fullscreen);
    m_navBar->setVisible(!fullscreen);
    if (fullscreen) {
        m_progressBar->hide();
        m_findBar->hide();
        showFullScreen();
    } else {
        showNormal();
        setZoomed(true);
    }
}

void MainWindow::onFullScreenRequested(QWebEngineFullScreenRequest request)
{
    request.accept();
    setFullscreen(request.toggleOn());
}

// === Downloads ===

void MainWindow::onDownloadRequested(QWebEngineDownloadRequest *download)
{
    // Nothing is written to disk unless the user picks a place for it
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Download",
        QDir(download->downloadDirectory()).filePath(download->downloadFileName()));
    if (path.isEmpty()) {
        download->cancel();
        return;
    }
    download->setDownloadDirectory(QFileInfo(path).path());
    download->setDownloadFileName(QFileInfo(path).fileName());
    download->accept();

    m_downloadLabel->setText("Downloading: " + download->downloadFileName());
    m_downloadProgress->setValue(0);
    m_downloadBar->show();

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this, [this, download]() {
        if (download->totalBytes() > 0) {
            m_downloadProgress->setValue(static_cast<int>((download->receivedBytes() * 100) / download->totalBytes()));
        }
    });

    connect(download, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, download]() {
        if (download->isFinished()) {
            m_downloadLabel->setText("Downloaded: " + download->downloadFileName());
            m_downloadProgress->setValue(100);

            // Auto-hide after 5 seconds
            QTimer::singleShot(5000, m_downloadBar, &QWidget::hide);
        }
    });
}

// === Print ===

void MainWindow::onPrint()
{
    if (auto *view = currentWebView()) {
        QString filePath = QFileDialog::getSaveFileName(
            this,
            "Save Page as PDF",
            QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("Slate.pdf"),
            "PDF Files (*.pdf)"
        );

        if (filePath.isEmpty()) {
            return;
        }
        if (!filePath.endsWith(".pdf", Qt::CaseInsensitive)) {
            filePath += ".pdf";
        }

        view->page()->printToPdf(filePath);
    }
}

// === Developer tools ===

void MainWindow::onToggleDevTools()
{
    if (m_devToolsView) {
        delete m_devToolsView;
        return;
    }

    if (auto *view = currentWebView()) {
        m_devToolsView = new QWebEngineView();
        m_devToolsView->setAttribute(Qt::WA_DeleteOnClose);
        m_devToolsView->setAttribute(Qt::WA_QuitOnClose, false);
        m_devToolsView->setWindowTitle("Slate DevTools");
        m_devToolsView->resize(900, 600);

        // Same session as the inspected tab, so nothing outlives it
        QWebEnginePage *devPage = new QWebEnginePage(view->page()->profile(), m_devToolsView);
        m_devToolsView->setPage(devPage);
        devPage->setInspectedPage(view->page());

        m_devToolsView->show();
    }
}

// === About page ===

void MainWindow::onAboutPage()
{
    addTab(createSession())->setHtml(
        internalPage("about")
            .replace("{{version}}", SLATE_VERSION)
            .replace("{{domains}}", QLocale().toString(m_blocklist->size())));
}

// === Window ===

bool MainWindow::isZoomed() const
{
    return IsZoomed(reinterpret_cast<HWND>(winId()));
}

// Qt's own maximize/restore for a frameless window loses the normal geometry
// when the window starts out maximized, so restore does nothing. Ask Windows.
void MainWindow::setZoomed(bool zoomed)
{
    ShowWindow(reinterpret_cast<HWND>(winId()), zoomed ? SW_MAXIMIZE : SW_RESTORE);
    // Single square = can maximize, overlapping squares = can restore
    m_maximizeButton->setText(zoomed ? "❐" : "□");
}

void MainWindow::applyTheme(bool isDark)
{
    m_isDarkMode = isDark;

    setStyleSheet(readResource(isDark ? ":/styles/dark.qss" : ":/styles/light.qss"));
    m_tabBar->setDarkMode(isDark);

    // Future pages
    for (QWebEngineProfile *session : findChildren<QWebEngineProfile*>(Qt::FindDirectChildrenOnly)) {
        installScrollbarScript(session);
    }

    // Pages that are already open
    for (int i = 0; i < m_stackedWidget->count(); ++i) {
        if (QWebEngineView *view = webViewAt(i)) {
            view->page()->runJavaScript(scrollbarInjectionScript(isDark));
            if (isInternalPage(view->url())) {
                view->page()->runJavaScript(
                    QString("document.body.className = '%1'").arg(isDark ? "dark" : "light"));
            }
        }
    }
}

void MainWindow::installScrollbarScript(QWebEngineProfile *session)
{
    for (const QWebEngineScript &existing : session->scripts()->find("slate-scrollbar")) {
        session->scripts()->remove(existing);
    }
    QWebEngineScript script;
    script.setName("slate-scrollbar");
    script.setSourceCode(scrollbarInjectionScript(m_isDarkMode));
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(true);
    session->scripts()->insert(script);
}

QString MainWindow::internalPage(const QString &name) const
{
    return readResource(":/pages/" + name + ".html")
        .replace("{{style}}", readResource(":/pages/page.css"))
        .replace("{{theme}}", m_isDarkMode ? "dark" : "light");
}

void MainWindow::showToast(const QString &message)
{
    m_toastLabel->setText(message);
    m_toastLabel->setFixedWidth(m_stackedWidget->width());
    m_toastLabel->move(0, 0);
    m_toastLabel->show();
    m_toastLabel->raise();

    // Auto-dismiss after 3 seconds
    QTimer::singleShot(3000, m_toastLabel, &QWidget::hide);
}

// Mouse events for window dragging
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Only drag from title bar area
        if (event->position().y() < m_titleBar->height()) {
            m_dragging = true;
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        if (isZoomed()) {
            setZoomed(false);
        }
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->position().y() < m_titleBar->height()) {
        setZoomed(!isZoomed());
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // Update tab bar with available width (window width minus window controls ~150px)
    m_tabBar->setAvailableWidth(event->size().width() - 150);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // DevTools is its own window; it must not outlive the session it inspects
    delete m_devToolsView;
    QMainWindow::closeEvent(event);
}

// A frameless window has no resize border, so report one to Windows:
// the outer few pixels of a non-maximized window act as resize handles.
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    MSG *msg = static_cast<MSG*>(message);
    if (msg->message != WM_NCHITTEST || msg->hwnd != reinterpret_cast<HWND>(winId())
        || isZoomed() || isFullScreen()) {
        return QMainWindow::nativeEvent(eventType, message, result);
    }

    RECT rect;
    GetWindowRect(msg->hwnd, &rect);
    const int x = GET_X_LPARAM(msg->lParam);
    const int y = GET_Y_LPARAM(msg->lParam);
    const int border = qRound(6 * devicePixelRatioF());

    const bool left = x < rect.left + border;
    const bool right = x >= rect.right - border;
    const bool top = y < rect.top + border;
    const bool bottom = y >= rect.bottom - border;

    if (top) {
        *result = left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP;
    } else if (bottom) {
        *result = left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM;
    } else if (left) {
        *result = HTLEFT;
    } else if (right) {
        *result = HTRIGHT;
    } else {
        return QMainWindow::nativeEvent(eventType, message, result);
    }
    return true;
}
