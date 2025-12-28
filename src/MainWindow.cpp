/**
 * MainWindow.cpp - Main browser window implementation
 */

#include "MainWindow.h"
#include <QFile>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineNewWindowRequest>
#include <QUrl>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QStyleOptionTab>
#include <QMenu>
#include <QApplication>
#include <QPropertyAnimation>

// Custom theme toggle switch
class ThemeToggle : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)
public:
    explicit ThemeToggle(QWidget *parent = nullptr) : QWidget(parent), m_position(0), m_isDark(false)
    {
        setFixedSize(44, 24);
        setCursor(Qt::PointingHandCursor);
        m_animation = new QPropertyAnimation(this, "position", this);
        m_animation->setDuration(150);
        m_animation->setEasingCurve(QEasingCurve::InOutQuad);
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

        // Track
        QColor trackColor = m_isDark ? QColor("#4a4a4a") : QColor("#e0e0e0");
        p.setBrush(trackColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(), height() / 2, height() / 2);

        // Thumb - simple circle, no icon
        int thumbSize = height() - 4;
        int thumbX = 2 + m_position * (width() - thumbSize - 4);
        QColor thumbColor = m_isDark ? QColor("#909090") : QColor("#ffffff");
        p.setBrush(thumbColor);
        p.setPen(Qt::NoPen);
        p.drawEllipse(thumbX, 2, thumbSize, thumbSize);
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

// Single welcome page HTML with smooth theme transitions via CSS
static const char* WELCOME_HTML = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif;
            display: flex;
            align-items: center;
            justify-content: center;
            height: 100vh;
            transition: background-color 0.25s ease, color 0.25s ease;
        }
        body.light {
            background-color: #ffffff;
        }
        body.dark {
            background-color: #1e1e1e;
        }
        .tagline {
            font-size: 24px;
            font-weight: 300;
            letter-spacing: 0.5px;
            transition: color 0.25s ease;
        }
        body.light .tagline {
            color: #a0a0a0;
        }
        body.dark .tagline {
            color: #606060;
        }
    </style>
</head>
<body class="light">
    <div class="tagline">Your blank slate.</div>
    <script>
        function setTheme(isDark) {
            document.body.className = isDark ? 'dark' : 'light';
        }
    </script>
</body>
</html>
)";

// Custom tab bar with fixed size tabs
SlateTabBar::SlateTabBar(QWidget *parent) : QTabBar(parent)
{
    setObjectName("slateTabBar");
    setExpanding(false);
    setDocumentMode(true);
    setTabsClosable(false);  // We draw our own close button
    setMovable(true);
    setElideMode(Qt::ElideRight);
    setMouseTracking(true);  // For hover effects
}

int SlateTabBar::calculateTabWidth() const
{
    int tabCount = count();
    if (tabCount == 0) return NORMAL_TAB_WIDTH;

    // For 5 or fewer tabs, use normal width
    if (tabCount <= TABS_BEFORE_SHRINK) {
        return NORMAL_TAB_WIDTH;
    }

    // Calculate available width for tabs (leave space for new tab button ~50px)
    int availableForTabs = m_availableWidth - 50;
    int calculatedWidth = availableForTabs / tabCount;

    // Clamp between min and normal (never larger than normal)
    return qBound(MIN_TAB_WIDTH, calculatedWidth, NORMAL_TAB_WIDTH);
}

QSize SlateTabBar::tabSizeHint(int index) const
{
    Q_UNUSED(index);
    int width = calculateTabWidth();
    return QSize(width, 46);
}

void SlateTabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    // Force recalculation of all tab sizes
    for (int i = 0; i < count(); ++i) {
        tabSizeHint(i);
    }
    updateGeometry();
    update();
}

void SlateTabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    // Force recalculation of all tab sizes
    updateGeometry();
    update();
}

void SlateTabBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < count(); ++i) {
        QStyleOptionTab opt;
        initStyleOption(&opt, i);

        QRect tabRectArea = tabRect(i);
        bool isSelected = (i == currentIndex());
        bool isHovered = tabRectArea.contains(mapFromGlobal(QCursor::pos()));

        // Draw tab background
        painter.setPen(Qt::NoPen);
        QRect bgRect = tabRectArea.adjusted(2, 3, -4, -3);  // Add horizontal spacing between tabs

        if (isSelected) {
            painter.setBrush(m_isDark ? QColor("#3c3c3c") : QColor("#ffffff"));
            painter.setPen(QPen(m_isDark ? QColor("#4a4a4a") : QColor("#e0e0e0"), 1));
        } else if (isHovered) {
            // Hover effect - same as selected but slightly different
            painter.setBrush(m_isDark ? QColor("#353535") : QColor("#f8f8f8"));
            painter.setPen(QPen(m_isDark ? QColor("#454545") : QColor("#e8e8e8"), 1));
        } else {
            painter.setBrush(Qt::NoBrush);
        }
        painter.drawRoundedRect(bgRect, 8, 8);

        // Calculate vertical center for consistent alignment
        int centerY = bgRect.center().y();

        // Draw close button first to get its rect
        int closeSize = 14;
        int closeX = bgRect.right() - closeSize - 10;
        int closeY = centerY - closeSize / 2;
        QRect closeRect(closeX, closeY, closeSize, closeSize);

        // Draw text - LEFT ALIGNED, vertically centered
        QRect textRect(bgRect.left() + 14, bgRect.top(), closeRect.left() - bgRect.left() - 20, bgRect.height());
        QColor textColor;
        if (m_isDark) {
            textColor = isSelected ? QColor("#e0e0e0") : QColor("#909090");
        } else {
            textColor = isSelected ? QColor("#1a1a1a") : QColor("#808080");
        }
        painter.setPen(textColor);
        painter.setFont(font());
        QString elidedText = painter.fontMetrics().elidedText(tabText(i), Qt::ElideRight, textRect.width());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);

        // Check if mouse is over close button
        QPoint mousePos = mapFromGlobal(QCursor::pos());
        bool closeHovered = closeRect.contains(mousePos);

        if (closeHovered) {
            painter.setBrush(m_isDark ? QColor("#505050") : QColor("#d4d4d4"));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(closeRect.adjusted(-2, -2, 2, 2), 4, 4);
        }

        // Draw X
        painter.setPen(QPen(m_isDark ? QColor("#909090") : QColor("#808080"), 1.5, Qt::SolidLine, Qt::RoundCap));
        int margin = 3;
        painter.drawLine(closeRect.left() + margin, closeRect.top() + margin,
                         closeRect.right() - margin, closeRect.bottom() - margin);
        painter.drawLine(closeRect.right() - margin, closeRect.top() + margin,
                         closeRect.left() + margin, closeRect.bottom() - margin);
    }
}

QRect SlateTabBar::closeButtonRect(int index) const
{
    QRect tabRectArea = tabRect(index);
    QRect bgRect = tabRectArea.adjusted(2, 3, -4, -3);
    int closeSize = 14;
    int closeX = bgRect.right() - closeSize - 10;
    int closeY = bgRect.center().y() - closeSize / 2;
    return QRect(closeX, closeY, closeSize, closeSize);
}

void SlateTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        for (int i = 0; i < count(); ++i) {
            if (closeButtonRect(i).contains(event->pos())) {
                emit tabCloseRequested(i);
                return;
            }
        }
    }
    QTabBar::mousePressEvent(event);
}

void SlateTabBar::mouseMoveEvent(QMouseEvent *event)
{
    update();  // Repaint for hover effects
    QTabBar::mouseMoveEvent(event);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Frameless window
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground, false);

    loadStylesheet();
    setupUI();
    setupShortcuts();
    setupConnections();
    setupTrayIcon();

    // Configure for privacy - no persistent storage
    auto *profile = QWebEngineProfile::defaultProfile();
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    // Set up ad blocker
    m_adBlocker = new AdBlocker(this);
    profile->setUrlRequestInterceptor(m_adBlocker);

    // Open initial tab
    onNewTab();

    // Set window properties
    setWindowTitle("Slate");
    resize(1200, 800);
    setMinimumSize(800, 600);

    // Start maximized
    showMaximized();
    m_maximizeButton->setText("❐");

    // Initialize tab bar width
    m_tabBar->setAvailableWidth(1200 - 150);  // Will be updated on resize
}

void MainWindow::loadStylesheet()
{
    QFile styleFile(":/styles/slate.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString style = styleFile.readAll();
        setStyleSheet(style);
        styleFile.close();
    }
}

void MainWindow::setupUI()
{
    // Central widget
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("centralWidget");
    setCentralWidget(m_centralWidget);

    // Main layout
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // === TITLE BAR (tabs + window controls) ===
    m_titleBar = new QWidget();
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(52);

    m_titleBarLayout = new QHBoxLayout(m_titleBar);
    m_titleBarLayout->setContentsMargins(12, 0, 0, 0);
    m_titleBarLayout->setSpacing(4);

    // Tab bar
    m_tabBar = new SlateTabBar();

    // New tab button
    m_newTabButton = new QPushButton("+");
    m_newTabButton->setObjectName("newTabButton");
    m_newTabButton->setFixedSize(28, 28);
    m_newTabButton->setToolTip("New Tab (Ctrl+T)");
    m_newTabButton->setCursor(Qt::PointingHandCursor);

    // Spacer to push window controls to right
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Window controls - Windows 11 style
    m_minimizeButton = new QPushButton("─");
    m_minimizeButton->setObjectName("windowButton");
    m_minimizeButton->setFixedSize(46, 52);
    m_minimizeButton->setToolTip("Minimize");
    m_minimizeButton->setCursor(Qt::PointingHandCursor);

    m_maximizeButton = new QPushButton("□");
    m_maximizeButton->setObjectName("windowButton");
    m_maximizeButton->setFixedSize(46, 52);
    m_maximizeButton->setToolTip("Maximize");
    m_maximizeButton->setCursor(Qt::PointingHandCursor);

    m_closeButton = new QPushButton("✕");
    m_closeButton->setObjectName("closeButton");
    m_closeButton->setFixedSize(46, 52);
    m_closeButton->setToolTip("Close");
    m_closeButton->setCursor(Qt::PointingHandCursor);

    m_titleBarLayout->addWidget(m_tabBar);
    m_titleBarLayout->addWidget(m_newTabButton);
    m_titleBarLayout->addWidget(spacer);
    m_titleBarLayout->addWidget(m_minimizeButton);
    m_titleBarLayout->addWidget(m_maximizeButton);
    m_titleBarLayout->addWidget(m_closeButton);

    // === NAVIGATION BAR ===
    m_navBar = new QWidget();
    m_navBar->setObjectName("navBar");
    m_navBar->setFixedHeight(54);

    m_navLayout = new QHBoxLayout(m_navBar);
    m_navLayout->setContentsMargins(12, 6, 12, 8);
    m_navLayout->setSpacing(6);

    // Back button - simple < > arrows
    m_backButton = new QPushButton();
    m_backButton->setObjectName("navButton");
    m_backButton->setText("<");
    m_backButton->setFixedSize(40, 40);
    m_backButton->setEnabled(false);
    m_backButton->setToolTip("Back");
    m_backButton->setCursor(Qt::PointingHandCursor);

    // Forward button
    m_forwardButton = new QPushButton();
    m_forwardButton->setObjectName("navButton");
    m_forwardButton->setText(">");
    m_forwardButton->setFixedSize(40, 40);
    m_forwardButton->setEnabled(false);
    m_forwardButton->setToolTip("Forward");
    m_forwardButton->setCursor(Qt::PointingHandCursor);

    // Reload button
    m_reloadButton = new QPushButton();
    m_reloadButton->setObjectName("navButton");
    m_reloadButton->setText("↻");
    m_reloadButton->setFixedSize(40, 40);
    m_reloadButton->setToolTip("Reload (Ctrl+R)");
    m_reloadButton->setCursor(Qt::PointingHandCursor);

    // Address bar
    m_addressBar = new AddressBar();

    // Theme toggle
    m_themeToggle = new ThemeToggle();

    m_navLayout->addWidget(m_backButton);
    m_navLayout->addWidget(m_forwardButton);
    m_navLayout->addWidget(m_reloadButton);
    m_navLayout->addSpacing(10);
    m_navLayout->addWidget(m_addressBar, 1);
    m_navLayout->addSpacing(10);
    m_navLayout->addWidget(m_themeToggle);

    // === CONTENT AREA ===
    m_stackedWidget = new QStackedWidget();
    m_stackedWidget->setObjectName("contentArea");

    // Add all to main layout
    m_mainLayout->addWidget(m_titleBar);
    m_mainLayout->addWidget(m_navBar);
    m_mainLayout->addWidget(m_stackedWidget, 1);
}

void MainWindow::setupShortcuts()
{
    m_newTabShortcut = new QShortcut(QKeySequence("Ctrl+T"), this);
    m_closeTabShortcut = new QShortcut(QKeySequence("Ctrl+W"), this);
    m_reloadShortcut = new QShortcut(QKeySequence("Ctrl+R"), this);
    m_focusAddressShortcut = new QShortcut(QKeySequence("Ctrl+L"), this);
    m_nextTabShortcut = new QShortcut(QKeySequence("Ctrl+Tab"), this);
    m_prevTabShortcut = new QShortcut(QKeySequence("Ctrl+Shift+Tab"), this);
}

void MainWindow::setupConnections()
{
    // Navigation buttons
    connect(m_backButton, &QPushButton::clicked, this, &MainWindow::onBack);
    connect(m_forwardButton, &QPushButton::clicked, this, &MainWindow::onForward);
    connect(m_reloadButton, &QPushButton::clicked, this, &MainWindow::onReload);
    connect(m_newTabButton, &QPushButton::clicked, this, &MainWindow::onNewTab);

    // Window controls
    connect(m_minimizeButton, &QPushButton::clicked, this, &MainWindow::onMinimize);
    connect(m_maximizeButton, &QPushButton::clicked, this, &MainWindow::onMaximizeRestore);
    connect(m_closeButton, &QPushButton::clicked, this, &MainWindow::onClose);

    // Address bar
    connect(m_addressBar, &AddressBar::urlEntered, this, &MainWindow::onNavigate);

    // Tab bar
    connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::onCloseTab);

    // Theme toggle
    connect(m_themeToggle, &ThemeToggle::toggled, this, &MainWindow::onThemeToggled);

    // Shortcuts
    connect(m_newTabShortcut, &QShortcut::activated, this, &MainWindow::onNewTab);
    connect(m_closeTabShortcut, &QShortcut::activated, [this]() {
        if (m_tabBar->count() > 1) {
            onCloseTab(m_tabBar->currentIndex());
        } else {
            close();
        }
    });
    connect(m_reloadShortcut, &QShortcut::activated, this, &MainWindow::onReload);
    connect(m_focusAddressShortcut, &QShortcut::activated, [this]() {
        m_addressBar->setFocus();
        m_addressBar->selectAll();
    });
    connect(m_nextTabShortcut, &QShortcut::activated, [this]() {
        int next = (m_tabBar->currentIndex() + 1) % m_tabBar->count();
        m_tabBar->setCurrentIndex(next);
    });
    connect(m_prevTabShortcut, &QShortcut::activated, [this]() {
        int prev = m_tabBar->currentIndex() - 1;
        if (prev < 0) prev = m_tabBar->count() - 1;
        m_tabBar->setCurrentIndex(prev);
    });
}

// Window control slots
void MainWindow::onMinimize()
{
    showMinimized();
}

void MainWindow::onMaximizeRestore()
{
    if (isMaximized()) {
        showNormal();
        m_maximizeButton->setText("□");  // Single square = can maximize
    } else {
        showMaximized();
        m_maximizeButton->setText("❐");  // Overlapping squares = can restore
    }
}

void MainWindow::onClose()
{
    close();
}

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);

    // Create a simple icon programmatically
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#1a1a1a"), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(4, 4, 24, 24);
    painter.setFont(QFont("Segoe UI", 14, QFont::Medium));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "S");
    painter.end();

    m_trayIcon->setIcon(QIcon(pixmap));
    m_trayIcon->setToolTip("Slate Browser");

    // Context menu
    QMenu *trayMenu = new QMenu(this);
    trayMenu->addAction("Show", this, [this]() {
        show();
        raise();
        activateWindow();
    });
    trayMenu->addAction("New Tab", this, &MainWindow::onNewTab);
    trayMenu->addSeparator();
    trayMenu->addAction("Exit", this, &QApplication::quit);
    m_trayIcon->setContextMenu(trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    m_trayIcon->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible() && !isMinimized()) {
            hide();
        } else {
            show();
            raise();
            activateWindow();
            if (isMinimized()) {
                showMaximized();
            }
        }
    }
}

void MainWindow::onThemeToggled(bool isDark)
{
    m_isDarkMode = isDark;
    applyTheme(isDark);
}

void MainWindow::applyTheme(bool isDark)
{
    QString style;
    if (isDark) {
        style = R"(
            QMainWindow, #centralWidget { background-color: #1e1e1e; }
            #titleBar { background-color: #2d2d2d; }
            #navBar { background-color: #1e1e1e; }
            #addressBar {
                background-color: #3c3c3c;
                border: none;
                border-radius: 18px;
                color: #e0e0e0;
                padding: 0px 16px;
                min-height: 36px;
                font-family: "SF Mono", "Consolas", "Monaco", monospace;
                font-size: 14px;
                selection-background-color: #264f78;
            }
            #addressBar:focus {
                background-color: #2d2d2d;
                border: 1px solid #4a4a4a;
            }
            #addressBar::placeholder { color: #808080; }
            #navButton {
                background-color: transparent;
                border: none;
                color: #b0b0b0;
                font-family: "Segoe UI", Arial, sans-serif;
                font-size: 22px;
                font-weight: 600;
                border-radius: 50%;
            }
            #navButton:hover { background-color: rgba(255, 255, 255, 0.1); }
            #navButton:pressed { background-color: rgba(255, 255, 255, 0.15); }
            #navButton:disabled { color: #505050; }
            #windowButton {
                background-color: transparent;
                border: none;
                color: #b0b0b0;
                font-family: "Segoe UI Symbol", "Segoe UI", sans-serif;
                font-size: 16px;
            }
            #windowButton:hover { background-color: #404040; }
            #closeButton {
                background-color: transparent;
                border: none;
                color: #b0b0b0;
                font-family: "Segoe UI Symbol", "Segoe UI", sans-serif;
                font-size: 16px;
            }
            #closeButton:hover { background-color: #e81123; color: #ffffff; }
            #newTabButton {
                background-color: transparent;
                border: none;
                color: #808080;
                font-size: 18px;
                font-weight: 300;
                border-radius: 6px;
            }
            #newTabButton:hover { background-color: #404040; color: #b0b0b0; }
            #contentArea { background-color: #1e1e1e; }
            #slateTabBar {
                background-color: transparent;
                border: none;
                font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif;
                font-size: 13px;
            }
        )";
    } else {
        // Load default light stylesheet from resources
        QFile styleFile(":/styles/slate.qss");
        if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
            style = styleFile.readAll();
            styleFile.close();
        }
    }
    setStyleSheet(style);

    // Update tab bar theme
    m_tabBar->setDarkMode(isDark);

    // Update welcome pages in existing tabs via JavaScript (smooth transition)
    for (int i = 0; i < m_stackedWidget->count(); ++i) {
        QWebEngineView *view = qobject_cast<QWebEngineView*>(m_stackedWidget->widget(i));
        if (view) {
            QString url = view->url().toString();
            // Check if it's a welcome page (about:blank or empty)
            if (url.isEmpty() || url == "about:blank" || url.startsWith("data:")) {
                view->page()->runJavaScript(QString("setTheme(%1)").arg(isDark ? "true" : "false"));
            }
        }
    }
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
        if (isMaximized()) {
            showNormal();
            m_maximizeButton->setText("□");
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
        onMaximizeRestore();
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // Update tab bar with available width (window width minus window controls ~150px)
    m_tabBar->setAvailableWidth(event->size().width() - 150);
}

void MainWindow::onNewTab()
{
    QWebEngineView *view = new QWebEngineView();
    view->setObjectName("webView");

    // Configure web settings
    QWebEngineSettings *settings = view->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);

    // Connect view signals
    connect(view, &QWebEngineView::urlChanged, this, &MainWindow::onUrlChanged);
    connect(view, &QWebEngineView::titleChanged, this, &MainWindow::onTitleChanged);

    // Handle new window requests (target="_blank" links)
    connect(view->page(), &QWebEnginePage::newWindowRequested, [this](QWebEngineNewWindowRequest &request) {
        onNewTab();
        if (auto *v = currentWebView()) {
            v->load(request.requestedUrl());
        }
    });

    // Add to stacked widget
    int stackIndex = m_stackedWidget->addWidget(view);

    // Add tab with "New Tab" title (not about:blank)
    int tabIndex = m_tabBar->addTab("New Tab");
    m_tabBar->setCurrentIndex(tabIndex);
    m_stackedWidget->setCurrentIndex(stackIndex);

    // Set background color to match theme BEFORE loading (prevents white flash)
    view->page()->setBackgroundColor(m_isDarkMode ? QColor("#1e1e1e") : QColor("#ffffff"));

    // Load welcome page with correct initial theme
    QString html = QString(WELCOME_HTML).replace("class=\"light\"",
        m_isDarkMode ? "class=\"dark\"" : "class=\"light\"");
    view->setHtml(html);

    // Focus address bar for new tabs
    m_addressBar->setFocus();
    m_addressBar->clear();
}

void MainWindow::onCloseTab(int index)
{
    if (m_tabBar->count() > 1) {
        m_tabBar->removeTab(index);
        QWidget *widget = m_stackedWidget->widget(index);
        m_stackedWidget->removeWidget(widget);
        widget->deleteLater();
    } else {
        close();
    }
}

void MainWindow::onTabChanged(int index)
{
    if (index >= 0 && index < m_stackedWidget->count()) {
        m_stackedWidget->setCurrentIndex(index);

        QWebEngineView *view = webViewAt(index);
        if (view) {
            QString urlStr = view->url().toString();
            if (urlStr.isEmpty() || urlStr == "about:blank" || urlStr.startsWith("data:")) {
                m_addressBar->clear();
            } else {
                // Show just the host for cleaner look
                QUrl url = view->url();
                m_addressBar->setText(url.host().isEmpty() ? urlStr : url.host());
            }
            updateNavigationButtons();
        }
    }
}

void MainWindow::onNavigate(const QString &url)
{
    QWebEngineView *view = currentWebView();
    if (view) {
        QString formattedUrl = formatUrl(url);
        view->load(QUrl(formattedUrl));
    }
}

void MainWindow::onUrlChanged(const QUrl &url)
{
    QWebEngineView *view = qobject_cast<QWebEngineView*>(sender());
    if (view && view == currentWebView()) {
        QString urlStr = url.toString();
        if (urlStr.isEmpty() || urlStr == "about:blank" || urlStr.startsWith("data:")) {
            m_addressBar->clear();
        } else {
            m_addressBar->setText(url.host().isEmpty() ? urlStr : url.host());
        }
        updateNavigationButtons();
    }
}

void MainWindow::onTitleChanged(const QString &title)
{
    QWebEngineView *view = qobject_cast<QWebEngineView*>(sender());
    if (view) {
        int index = m_stackedWidget->indexOf(view);
        if (index >= 0 && index < m_tabBar->count()) {
            QString displayTitle = title;

            // Don't show about:blank, show "New Tab" instead
            if (displayTitle.isEmpty() || displayTitle == "about:blank") {
                displayTitle = "New Tab";
            }

            // Keep the full title - let the tab bar elide it
            m_tabBar->setTabText(index, displayTitle);
        }
    }
}

void MainWindow::onBack()
{
    if (auto *view = currentWebView()) {
        view->back();
    }
}

void MainWindow::onForward()
{
    if (auto *view = currentWebView()) {
        view->forward();
    }
}

void MainWindow::onReload()
{
    if (auto *view = currentWebView()) {
        view->reload();
    }
}

void MainWindow::updateNavigationButtons()
{
    if (auto *view = currentWebView()) {
        m_backButton->setEnabled(view->history()->canGoBack());
        m_forwardButton->setEnabled(view->history()->canGoForward());
    }
}

QWebEngineView* MainWindow::currentWebView() const
{
    return qobject_cast<QWebEngineView*>(m_stackedWidget->currentWidget());
}

QWebEngineView* MainWindow::webViewAt(int index) const
{
    return qobject_cast<QWebEngineView*>(m_stackedWidget->widget(index));
}

QString MainWindow::formatUrl(const QString &input)
{
    QString url = input.trimmed();

    if (!url.contains('.') || url.contains(' ')) {
        return "https://duckduckgo.com/?q=" + QUrl::toPercentEncoding(url);
    }

    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        url = "https://" + url;
    }

    return url;
}
