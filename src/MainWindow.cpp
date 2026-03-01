/**
 * MainWindow.cpp - Main browser window implementation
 */

#include "MainWindow.h"
#include <QFile>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineProfileBuilder>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineCertificateError>
#include <QUrl>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QStyleOptionTab>
#include <QMenu>
#include <QApplication>
#include <QPropertyAnimation>
#include <QTimer>
#include <QToolTip>
#include <QCompleter>
#include <QStringListModel>
#include <QLabel>
#include <QProgressBar>
#include <QLineEdit>
#include <QWebEngineDownloadRequest>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QSplitter>
#include <QDesktopServices>
#include "Version.h"

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

        // Draw favicon if present
        int textLeftOffset = 14;
        QIcon favicon = m_favicons.value(i, QIcon());
        if (!favicon.isNull()) {
            int iconSize = 16;
            int iconX = bgRect.left() + 10;
            int iconY = centerY - iconSize / 2;
            favicon.paint(&painter, iconX, iconY, iconSize, iconSize);
            textLeftOffset = 32;  // Push text right to make room for icon
        }

        // Draw text - LEFT ALIGNED, vertically centered
        QRect textRect(bgRect.left() + textLeftOffset, bgRect.top(), closeRect.left() - bgRect.left() - textLeftOffset - 6, bgRect.height());
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
    if (event->button() == Qt::MiddleButton) {
        int index = tabAt(event->pos());
        if (index >= 0) {
            emit tabCloseRequested(index);
            return;
        }
    }
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

void SlateTabBar::setTabFavicon(int index, const QIcon &icon)
{
    m_favicons[index] = icon;
    update();
}

QIcon SlateTabBar::tabFavicon(int index) const
{
    return m_favicons.value(index, QIcon());
}

void SlateTabBar::mouseMoveEvent(QMouseEvent *event)
{
    int index = tabAt(event->pos());
    if (index != m_lastHoverIndex) {
        m_lastHoverIndex = index;
        if (index >= 0) {
            QToolTip::showText(mapToGlobal(event->pos()), tabText(index), this);
        } else {
            QToolTip::hideText();
        }
    }
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

    // Default to dark mode on launch
    m_themeToggle->setDark(true);

    // Set window properties
    setWindowTitle("Slate");
    resize(1200, 800);
    setMinimumSize(800, 600);

    // Start maximized
    showMaximized();
    m_maximizeButton->setText("❐");

    // Initialize tab bar width
    m_tabBar->setAvailableWidth(1200 - 150);  // Will be updated on resize

    // Defer WebEngine initialization to speed up window appearance
    QTimer::singleShot(0, this, &MainWindow::initializeWebEngine);
    QTimer::singleShot(0, this, &MainWindow::setupTrayIcon);
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
    m_urlModel = new QStringListModel(this);
    m_urlCompleter = new QCompleter(m_urlModel, this);
    m_urlCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_urlCompleter->setFilterMode(Qt::MatchContains);
    m_urlCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_addressBar->setCompleter(m_urlCompleter);

    // Theme toggle
    m_themeToggle = new ThemeToggle();

    m_navLayout->addWidget(m_backButton);
    m_navLayout->addWidget(m_forwardButton);
    m_navLayout->addWidget(m_reloadButton);
    m_navLayout->addSpacing(10);
    m_navLayout->addWidget(m_addressBar, 1);
    m_navLayout->addSpacing(10);
    m_navLayout->addWidget(m_themeToggle);

    // === PROGRESS BAR (Feature 3) ===
    m_progressBar = new QProgressBar();
    m_progressBar->setObjectName("loadingBar");
    m_progressBar->setFixedHeight(3);
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
    m_stackedWidget->setCurrentWidget(m_placeholderWidget);

    // === PERMISSION TOAST (Feature 9) ===
    m_toastLabel = new QLabel(m_stackedWidget);
    m_toastLabel->setObjectName("permissionToast");
    m_toastLabel->setFixedHeight(36);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->hide();

    // === FIND BAR (Feature 1) ===
    setupFindBar();

    // === DOWNLOAD BAR (Feature 10) ===
    m_downloadBar = new QWidget();
    m_downloadBar->setObjectName("downloadBar");
    m_downloadBar->setFixedHeight(44);
    m_downloadBar->hide();

    QHBoxLayout *dlLayout = new QHBoxLayout(m_downloadBar);
    dlLayout->setContentsMargins(12, 4, 12, 4);
    dlLayout->setSpacing(8);

    m_downloadLabel = new QLabel();
    m_downloadLabel->setObjectName("downloadLabel");

    m_downloadProgress = new QProgressBar();
    m_downloadProgress->setObjectName("downloadProgress");
    m_downloadProgress->setFixedWidth(200);
    m_downloadProgress->setFixedHeight(6);
    m_downloadProgress->setTextVisible(false);
    m_downloadProgress->setRange(0, 100);

    m_downloadCloseBtn = new QPushButton("✕");
    m_downloadCloseBtn->setObjectName("findCloseButton");
    m_downloadCloseBtn->setFixedSize(32, 32);
    m_downloadCloseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_downloadCloseBtn, &QPushButton::clicked, m_downloadBar, &QWidget::hide);

    dlLayout->addWidget(m_downloadLabel);
    dlLayout->addWidget(m_downloadProgress);
    dlLayout->addStretch();
    dlLayout->addWidget(m_downloadCloseBtn);

    // Add all to main layout
    m_mainLayout->addWidget(m_titleBar);
    m_mainLayout->addWidget(m_navBar);
    m_mainLayout->addWidget(m_progressBar);
    m_mainLayout->addWidget(m_stackedWidget, 1);
    m_mainLayout->addWidget(m_findBar);
    m_mainLayout->addWidget(m_downloadBar);
}

void MainWindow::setupShortcuts()
{
    m_newTabShortcut = new QShortcut(QKeySequence("Ctrl+T"), this);
    m_closeTabShortcut = new QShortcut(QKeySequence("Ctrl+W"), this);
    m_reloadShortcut = new QShortcut(QKeySequence("Ctrl+R"), this);
    m_focusAddressShortcut = new QShortcut(QKeySequence("Ctrl+L"), this);
    m_nextTabShortcut = new QShortcut(QKeySequence("Ctrl+Tab"), this);
    m_prevTabShortcut = new QShortcut(QKeySequence("Ctrl+Shift+Tab"), this);

    // Feature 1: Find in page
    m_findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);

    // Feature 2: Zoom
    m_zoomInShortcut = new QShortcut(QKeySequence("Ctrl++"), this);
    m_zoomInShortcut2 = new QShortcut(QKeySequence("Ctrl+="), this);
    m_zoomOutShortcut = new QShortcut(QKeySequence("Ctrl+-"), this);
    m_zoomResetShortcut = new QShortcut(QKeySequence("Ctrl+0"), this);

    // Feature 5: Fullscreen
    m_fullscreenShortcut = new QShortcut(QKeySequence("F11"), this);
    m_escapeFullscreenShortcut = new QShortcut(QKeySequence("Escape"), this);

    // Feature 11: Print
    m_printShortcut = new QShortcut(QKeySequence("Ctrl+P"), this);

    // Feature 12: DevTools
    m_devToolsShortcut = new QShortcut(QKeySequence("F12"), this);
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
    connect(m_addressBar, &AddressBar::focusGained, this, &MainWindow::onAddressFocusGained);
    connect(m_addressBar, &AddressBar::focusLost, this, &MainWindow::onAddressFocusLost);

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

    // Feature 1: Find in page
    connect(m_findShortcut, &QShortcut::activated, this, &MainWindow::onFindInPage);

    // Feature 2: Zoom
    connect(m_zoomInShortcut, &QShortcut::activated, this, &MainWindow::onZoomIn);
    connect(m_zoomInShortcut2, &QShortcut::activated, this, &MainWindow::onZoomIn);
    connect(m_zoomOutShortcut, &QShortcut::activated, this, &MainWindow::onZoomOut);
    connect(m_zoomResetShortcut, &QShortcut::activated, this, &MainWindow::onZoomReset);

    // Feature 5: Fullscreen
    connect(m_fullscreenShortcut, &QShortcut::activated, this, &MainWindow::onToggleFullscreen);
    connect(m_escapeFullscreenShortcut, &QShortcut::activated, [this]() {
        if (m_isFullscreen) onToggleFullscreen();
        else if (m_findBar->isVisible()) onFindClose();
    });

    // Feature 11: Print
    connect(m_printShortcut, &QShortcut::activated, this, &MainWindow::onPrint);

    // Feature 12: DevTools
    connect(m_devToolsShortcut, &QShortcut::activated, this, &MainWindow::onToggleDevTools);
}

void MainWindow::initializeWebEngine()
{
    if (m_webEngineInitialized) {
        return;
    }
    m_webEngineInitialized = true;

    // Configure for privacy - off-the-record profile, no persistent storage
    m_profile = QWebEngineProfileBuilder::createOffTheRecordProfile(this);
    m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    m_profile->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);

    // Set up ad blocker
    m_adBlocker = new AdBlocker(this);
    m_profile->setUrlRequestInterceptor(m_adBlocker);

    // Feature 10: Download handling
    connect(m_profile, &QWebEngineProfile::downloadRequested, this, &MainWindow::onDownloadRequested);

    // Open initial tab
    onNewTab();
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
    trayMenu->addAction("About Slate", this, &MainWindow::onAboutPage);
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
            #loadingBar {
                background-color: transparent;
                border: none;
                max-height: 3px;
            }
            #loadingBar::chunk {
                background-color: #4a9eff;
                border-radius: 1px;
            }
            #findBar {
                background-color: #2d2d2d;
                border-top: 1px solid #3c3c3c;
            }
            #findEdit {
                background-color: #3c3c3c;
                border: 1px solid #4a4a4a;
                border-radius: 6px;
                color: #e0e0e0;
                padding: 0px 10px;
                font-family: "Segoe UI", sans-serif;
                font-size: 13px;
            }
            #findEdit:focus { border-color: #4a9eff; }
            #findLabel { color: #909090; font-size: 12px; }
            #findButton {
                background-color: transparent;
                border: none;
                border-radius: 6px;
                color: #b0b0b0;
                font-size: 12px;
            }
            #findButton:hover { background-color: #404040; }
            #findCloseButton {
                background-color: transparent;
                border: none;
                border-radius: 6px;
                color: #909090;
                font-size: 14px;
            }
            #findCloseButton:hover { background-color: #404040; color: #e0e0e0; }
            #permissionToast {
                background-color: #2d2d2d;
                color: #e0e0e0;
                border-bottom: 1px solid #3c3c3c;
                font-family: "Segoe UI", sans-serif;
                font-size: 13px;
                padding: 0px 16px;
            }
            #downloadBar {
                background-color: #2d2d2d;
                border-top: 1px solid #3c3c3c;
            }
            #downloadLabel {
                color: #e0e0e0;
                font-family: "Segoe UI", sans-serif;
                font-size: 13px;
            }
            #downloadProgress {
                background-color: #3c3c3c;
                border: none;
                border-radius: 3px;
            }
            #downloadProgress::chunk {
                background-color: #4a9eff;
                border-radius: 3px;
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
    QWebEnginePage *page = new QWebEnginePage(m_profile, view);
    view->setPage(page);

    // Configure web settings
    QWebEngineSettings *settings = view->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

    // Deny permission requests by default for privacy, with visible notification
    connect(page, &QWebEnginePage::featurePermissionRequested,
        [this, page](const QUrl &securityOrigin, QWebEnginePage::Feature feature) {
            page->setFeaturePermission(securityOrigin, feature, QWebEnginePage::PermissionDeniedByUser);

            // Show toast with human-readable feature name
            QString featureName;
            switch (feature) {
                case QWebEnginePage::MediaAudioCapture: featureName = "Microphone"; break;
                case QWebEnginePage::MediaVideoCapture: featureName = "Camera"; break;
                case QWebEnginePage::MediaAudioVideoCapture: featureName = "Camera & Microphone"; break;
                case QWebEnginePage::Geolocation: featureName = "Location"; break;
                case QWebEnginePage::DesktopVideoCapture: featureName = "Screen sharing"; break;
                case QWebEnginePage::DesktopAudioVideoCapture: featureName = "Screen sharing"; break;
                case QWebEnginePage::Notifications: featureName = "Notifications"; break;
                case QWebEnginePage::ClipboardReadWrite: featureName = "Clipboard"; break;
                default: featureName = "Permission"; break;
            }
            showPermissionToast(featureName + " blocked — " + securityOrigin.host());
        });

    // Connect view signals
    connect(view, &QWebEngineView::urlChanged, this, &MainWindow::onUrlChanged);
    connect(view, &QWebEngineView::titleChanged, this, &MainWindow::onTitleChanged);

    // Feature 3: Loading progress
    connect(view, &QWebEngineView::loadStarted, this, &MainWindow::onLoadStarted);
    connect(view, &QWebEngineView::loadProgress, this, &MainWindow::onLoadProgress);
    connect(view, &QWebEngineView::loadFinished, this, &MainWindow::onLoadFinished);

    // Feature 4: Favicon
    connect(view, &QWebEngineView::iconChanged, this, &MainWindow::onIconChanged);

    // Feature 5: Fullscreen support for HTML5 video
    connect(page, &QWebEnginePage::fullScreenRequested, this, &MainWindow::onFullScreenRequested);

    // Feature 8: Certificate error handling — reject and show warning
    connect(page, &QWebEnginePage::certificateError, [this, view](QWebEngineCertificateError error) {
        error.rejectCertificate();
        QString errorHtml = QString(R"(
            <!DOCTYPE html><html><head><style>
            * { margin: 0; padding: 0; box-sizing: border-box; }
            body { font-family: -apple-system, "Segoe UI", sans-serif; display: flex;
                   align-items: center; justify-content: center; height: 100vh;
                   background-color: %1; color: %2; }
            .container { text-align: center; max-width: 500px; padding: 40px; }
            .icon { font-size: 64px; margin-bottom: 20px; }
            h1 { font-size: 22px; font-weight: 600; margin-bottom: 12px; }
            p { font-size: 14px; color: %3; line-height: 1.6; }
            .url { font-family: monospace; font-size: 13px; color: %4;
                   background: %5; padding: 4px 8px; border-radius: 4px; margin-top: 16px; display: inline-block; }
            </style></head><body>
            <div class="container">
                <div class="icon">⚠</div>
                <h1>Connection Not Secure</h1>
                <p>Slate blocked this connection because the site's certificate is not trusted. This protects your data from being intercepted.</p>
                <div class="url">%6</div>
            </div></body></html>
        )").arg(
            m_isDarkMode ? "#1e1e1e" : "#ffffff",
            m_isDarkMode ? "#e0e0e0" : "#303030",
            m_isDarkMode ? "#909090" : "#606060",
            m_isDarkMode ? "#e0a030" : "#b07020",
            m_isDarkMode ? "#2d2d2d" : "#f5f5f5",
            error.url().host().toHtmlEscaped()
        );
        view->setHtml(errorHtml, error.url());
    });

    // Handle new window requests (target="_blank" links)
    connect(view->page(), &QWebEnginePage::newWindowRequested, [this](QWebEngineNewWindowRequest &request) {
        onNewTab();
        if (auto *v = currentWebView()) {
            v->load(request.requestedUrl());
        }
    });

    // Add to stacked widget
    int stackIndex = m_stackedWidget->addWidget(view);

    if (m_placeholderWidget) {
        m_stackedWidget->removeWidget(m_placeholderWidget);
        m_placeholderWidget->deleteLater();
        m_placeholderWidget = nullptr;
    }

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
        addToHistory(url);
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

void MainWindow::onAddressFocusGained()
{
    if (auto *view = currentWebView()) {
        QUrl url = view->url();
        QString urlStr = url.toString();
        if (!urlStr.isEmpty() && urlStr != "about:blank" && !urlStr.startsWith("data:")) {
            m_addressBar->setText(urlStr);
        }
    }
}

void MainWindow::onAddressFocusLost()
{
    if (auto *view = currentWebView()) {
        QUrl url = view->url();
        QString urlStr = url.toString();
        if (urlStr.isEmpty() || urlStr == "about:blank" || urlStr.startsWith("data:")) {
            m_addressBar->clear();
        } else {
            m_addressBar->setText(url.host().isEmpty() ? urlStr : url.host());
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

void MainWindow::addToHistory(const QUrl &url)
{
    if (!url.isValid()) {
        return;
    }
    QString scheme = url.scheme().toLower();
    if (scheme != "http" && scheme != "https") {
        return;
    }
    QString urlStr = url.toString();
    if (urlStr.isEmpty() || urlStr == "about:blank" || urlStr.startsWith("data:")) {
        return;
    }
    if (!m_urlHistorySet.contains(urlStr)) {
        m_urlHistorySet.insert(urlStr);
        m_urlHistory.append(urlStr);
        if (m_urlModel) {
            m_urlModel->setStringList(m_urlHistory);
        }
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

// === FEATURE 1: Find in Page ===

void MainWindow::setupFindBar()
{
    m_findBar = new QWidget();
    m_findBar->setObjectName("findBar");
    m_findBar->setFixedHeight(44);
    m_findBar->hide();

    QHBoxLayout *findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(12, 4, 12, 4);
    findLayout->setSpacing(6);

    m_findEdit = new QLineEdit();
    m_findEdit->setObjectName("findEdit");
    m_findEdit->setPlaceholderText("Find in page...");
    m_findEdit->setFixedWidth(280);
    m_findEdit->setFixedHeight(32);

    m_findLabel = new QLabel();
    m_findLabel->setObjectName("findLabel");

    m_findPrevBtn = new QPushButton("▲");
    m_findPrevBtn->setObjectName("findButton");
    m_findPrevBtn->setFixedSize(32, 32);
    m_findPrevBtn->setToolTip("Previous match");
    m_findPrevBtn->setCursor(Qt::PointingHandCursor);

    m_findNextBtn = new QPushButton("▼");
    m_findNextBtn->setObjectName("findButton");
    m_findNextBtn->setFixedSize(32, 32);
    m_findNextBtn->setToolTip("Next match");
    m_findNextBtn->setCursor(Qt::PointingHandCursor);

    m_findCloseBtn = new QPushButton("✕");
    m_findCloseBtn->setObjectName("findCloseButton");
    m_findCloseBtn->setFixedSize(32, 32);
    m_findCloseBtn->setToolTip("Close (Esc)");
    m_findCloseBtn->setCursor(Qt::PointingHandCursor);

    findLayout->addWidget(m_findEdit);
    findLayout->addWidget(m_findLabel);
    findLayout->addWidget(m_findPrevBtn);
    findLayout->addWidget(m_findNextBtn);
    findLayout->addStretch();
    findLayout->addWidget(m_findCloseBtn);

    connect(m_findEdit, &QLineEdit::textChanged, this, &MainWindow::onFindTextChanged);
    connect(m_findEdit, &QLineEdit::returnPressed, this, &MainWindow::onFindNext);
    connect(m_findNextBtn, &QPushButton::clicked, this, &MainWindow::onFindNext);
    connect(m_findPrevBtn, &QPushButton::clicked, this, &MainWindow::onFindPrev);
    connect(m_findCloseBtn, &QPushButton::clicked, this, &MainWindow::onFindClose);
}

void MainWindow::onFindInPage()
{
    m_findBar->setVisible(!m_findBar->isVisible());
    if (m_findBar->isVisible()) {
        m_findEdit->setFocus();
        m_findEdit->selectAll();
    } else {
        // Clear highlights when closing
        if (auto *view = currentWebView()) {
            view->findText(QString());
        }
    }
}

void MainWindow::onFindNext()
{
    if (auto *view = currentWebView()) {
        view->findText(m_findEdit->text());
    }
}

void MainWindow::onFindPrev()
{
    if (auto *view = currentWebView()) {
        view->findText(m_findEdit->text(), QWebEnginePage::FindBackward);
    }
}

void MainWindow::onFindClose()
{
    m_findBar->hide();
    if (auto *view = currentWebView()) {
        view->findText(QString());  // Clear highlights
    }
}

void MainWindow::onFindTextChanged(const QString &text)
{
    if (auto *view = currentWebView()) {
        view->findText(text);
    }
}

// === FEATURE 2: Zoom Controls ===

void MainWindow::onZoomIn()
{
    if (auto *view = currentWebView()) {
        qreal zoom = view->zoomFactor();
        if (zoom < 5.0) {
            view->setZoomFactor(zoom + 0.1);
        }
    }
}

void MainWindow::onZoomOut()
{
    if (auto *view = currentWebView()) {
        qreal zoom = view->zoomFactor();
        if (zoom > 0.25) {
            view->setZoomFactor(zoom - 0.1);
        }
    }
}

void MainWindow::onZoomReset()
{
    if (auto *view = currentWebView()) {
        view->setZoomFactor(1.0);
    }
}

// === FEATURE 3: Loading Progress ===

void MainWindow::onLoadStarted()
{
    QWebEngineView *view = qobject_cast<QWebEngineView*>(sender());
    if (view && view == currentWebView()) {
        m_progressBar->setValue(0);
        m_progressBar->show();
    }
}

void MainWindow::onLoadProgress(int progress)
{
    QWebEngineView *view = qobject_cast<QWebEngineView*>(sender());
    if (view && view == currentWebView()) {
        m_progressBar->setValue(progress);
    }
}

void MainWindow::onLoadFinished(bool ok)
{
    Q_UNUSED(ok);
    QWebEngineView *view = qobject_cast<QWebEngineView*>(sender());
    if (view && view == currentWebView()) {
        m_progressBar->hide();
    }
}

// === FEATURE 4: Favicon ===

void MainWindow::onIconChanged(const QIcon &icon)
{
    QWebEngineView *view = qobject_cast<QWebEngineView*>(sender());
    if (view) {
        int index = m_stackedWidget->indexOf(view);
        if (index >= 0 && index < m_tabBar->count()) {
            m_tabBar->setTabFavicon(index, icon);
        }
    }
}

// === FEATURE 5: Fullscreen ===

void MainWindow::onToggleFullscreen()
{
    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen) {
        m_titleBar->hide();
        m_navBar->hide();
        m_progressBar->hide();
        m_findBar->hide();
        showFullScreen();
    } else {
        m_titleBar->show();
        m_navBar->show();
        showMaximized();
        m_maximizeButton->setText("❐");
    }
}

void MainWindow::onFullScreenRequested(QWebEngineFullScreenRequest request)
{
    request.accept();

    if (request.toggleOn()) {
        if (!m_isFullscreen) {
            m_isFullscreen = true;
            m_titleBar->hide();
            m_navBar->hide();
            m_progressBar->hide();
            m_findBar->hide();
            showFullScreen();
        }
    } else {
        if (m_isFullscreen) {
            m_isFullscreen = false;
            m_titleBar->show();
            m_navBar->show();
            showMaximized();
            m_maximizeButton->setText("❐");
        }
    }
}

// === FEATURE 10: Download Manager ===

void MainWindow::onDownloadRequested(QWebEngineDownloadRequest *download)
{
    // Accept the download to default location
    download->accept();

    QString fileName = download->downloadFileName();
    m_downloadLabel->setText("Downloading: " + fileName);
    m_downloadProgress->setValue(0);
    m_downloadBar->show();

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, [this, download]() {
        if (download->totalBytes() > 0) {
            int percent = static_cast<int>((download->receivedBytes() * 100) / download->totalBytes());
            m_downloadProgress->setValue(percent);
        }
    });

    connect(download, &QWebEngineDownloadRequest::isFinishedChanged, [this, download]() {
        if (download->isFinished()) {
            QString dir = download->downloadDirectory();
            m_downloadLabel->setText("Downloaded: " + download->downloadFileName());
            m_downloadProgress->setValue(100);

            // Auto-hide after 5 seconds
            QTimer::singleShot(5000, m_downloadBar, &QWidget::hide);
        }
    });
}

// === FEATURE 11: Print ===

void MainWindow::onPrint()
{
    if (auto *view = currentWebView()) {
        QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (defaultDir.isEmpty()) {
            defaultDir = QDir::homePath();
        }

        QString filePath = QFileDialog::getSaveFileName(
            this,
            "Save Page as PDF",
            QDir(defaultDir).filePath("Slate.pdf"),
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

// === FEATURE 12: DevTools ===

void MainWindow::onToggleDevTools()
{
    if (auto *view = currentWebView()) {
        if (m_devToolsOpen && m_devToolsView) {
            // Close devtools
            m_devToolsView->page()->setInspectedPage(nullptr);
            m_devToolsView->hide();
            m_devToolsView->deleteLater();
            m_devToolsView = nullptr;
            m_devToolsOpen = false;
        } else {
            // Open devtools in a new window (lightweight approach)
            m_devToolsView = new QWebEngineView();
            m_devToolsView->setWindowTitle("Slate DevTools");
            m_devToolsView->resize(900, 600);

            QWebEnginePage *devPage = new QWebEnginePage(m_profile, m_devToolsView);
            m_devToolsView->setPage(devPage);
            devPage->setInspectedPage(view->page());

            m_devToolsView->show();
            m_devToolsOpen = true;

            // Track window close
            connect(m_devToolsView, &QWebEngineView::destroyed, [this]() {
                m_devToolsView = nullptr;
                m_devToolsOpen = false;
            });
        }
    }
}

// === FEATURE 13: About Page ===

void MainWindow::onAboutPage()
{
    if (auto *view = currentWebView()) {
        QString aboutHtml = QString(R"(
            <!DOCTYPE html><html><head><style>
            * { margin: 0; padding: 0; box-sizing: border-box; }
            body { font-family: -apple-system, "Segoe UI", system-ui, sans-serif;
                   display: flex; align-items: center; justify-content: center;
                   height: 100vh; background-color: %1; color: %2; }
            .container { text-align: center; max-width: 600px; padding: 40px; }
            h1 { font-size: 36px; font-weight: 200; letter-spacing: 4px; margin-bottom: 8px; }
            .version { font-size: 14px; color: %3; margin-bottom: 32px; }
            .features { text-align: left; margin: 0 auto; max-width: 400px; }
            .features h3 { font-size: 14px; font-weight: 600; color: %4;
                           text-transform: uppercase; letter-spacing: 1px; margin: 20px 0 10px; }
            .features .item { font-size: 13px; color: %3; padding: 4px 0; display: flex;
                              justify-content: space-between; }
            .features .key { font-family: "SF Mono", Consolas, monospace; font-size: 12px;
                             background: %5; padding: 2px 8px; border-radius: 4px; color: %4; }
            </style></head><body>
            <div class="container">
                <h1>SLATE</h1>
                <div class="version">v%6 — A clean slate for web browsing</div>
                <div class="features">
                    <h3>Keyboard Shortcuts</h3>
                    <div class="item"><span>New Tab</span><span class="key">Ctrl+T</span></div>
                    <div class="item"><span>Close Tab</span><span class="key">Ctrl+W</span></div>
                    <div class="item"><span>Focus Address Bar</span><span class="key">Ctrl+L</span></div>
                    <div class="item"><span>Reload</span><span class="key">Ctrl+R</span></div>
                    <div class="item"><span>Find in Page</span><span class="key">Ctrl+F</span></div>
                    <div class="item"><span>Zoom In / Out / Reset</span><span class="key">Ctrl+/- / 0</span></div>
                    <div class="item"><span>Next / Prev Tab</span><span class="key">Ctrl+Tab</span></div>
                    <div class="item"><span>Fullscreen</span><span class="key">F11</span></div>
                    <div class="item"><span>Print</span><span class="key">Ctrl+P</span></div>
                    <div class="item"><span>Developer Tools</span><span class="key">F12</span></div>
                    <h3>Privacy</h3>
                    <div class="item"><span>HTTPS-only mode</span><span class="key">Always on</span></div>
                    <div class="item"><span>Ad & tracker blocking</span><span class="key">Always on</span></div>
                    <div class="item"><span>No history saved</span><span class="key">By design</span></div>
                    <div class="item"><span>No cookies persist</span><span class="key">By design</span></div>
                </div>
            </div></body></html>
        )").arg(
            m_isDarkMode ? "#1e1e1e" : "#ffffff",
            m_isDarkMode ? "#e0e0e0" : "#303030",
            m_isDarkMode ? "#909090" : "#606060",
            m_isDarkMode ? "#c0c0c0" : "#404040",
            m_isDarkMode ? "#2d2d2d" : "#f0f0f0",
            QString(SLATE_VERSION)
        );
        view->setHtml(aboutHtml);
        int index = m_stackedWidget->indexOf(view);
        if (index >= 0) {
            m_tabBar->setTabText(index, "About Slate");
        }
    }
}

// === FEATURE 9: Permission Toast ===

void MainWindow::showPermissionToast(const QString &message)
{
    m_toastLabel->setText("  🛡  " + message);
    m_toastLabel->setFixedWidth(m_stackedWidget->width());
    m_toastLabel->move(0, 0);
    m_toastLabel->show();
    m_toastLabel->raise();

    // Auto-dismiss after 3 seconds
    QTimer::singleShot(3000, m_toastLabel, &QWidget::hide);
}

QString MainWindow::formatUrl(const QString &input)
{
    QString url = input.trimmed();

    if (!url.contains('.') || url.contains(' ')) {
        return "https://duckduckgo.com/?q=" + QUrl::toPercentEncoding(url);
    }

    // HTTPS-only: always upgrade http to https
    if (url.startsWith("http://")) {
        url = "https://" + url.mid(7);
    } else if (!url.startsWith("https://")) {
        url = "https://" + url;
    }

    return url;
}
