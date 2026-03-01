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
#include <QLabel>
#include <QProgressBar>
#include <QLineEdit>
#include <QWebEngineDownloadRequest>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QSplitter>
#include <QDesktopServices>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include "Version.h"

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

// Welcome page — refined entrance with staggered fade-in
static const char* WELCOME_HTML = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=DM+Sans:ital,opsz,wght@0,9..40,100..1000;1,9..40,100..1000&display=swap');

        *, *::before, *::after { margin: 0; padding: 0; box-sizing: border-box; }

        body {
            font-family: 'DM Sans', -apple-system, 'Segoe UI', system-ui, sans-serif;
            display: flex;
            align-items: center;
            justify-content: center;
            height: 100vh;
            overflow: hidden;
            transition: background-color 0.35s ease, color 0.35s ease;
        }

        body.light { background-color: #fafaf9; }
        body.dark  { background-color: #161619; }

        .welcome {
            text-align: center;
            animation: enterUp 0.6s cubic-bezier(0.16, 1, 0.3, 1) both;
        }

        .tagline {
            font-size: 28px;
            font-weight: 300;
            letter-spacing: 3px;
            text-transform: lowercase;
            transition: color 0.35s ease;
            opacity: 0;
            animation: fadeIn 0.8s ease 0.15s forwards;
        }

        .accent {
            display: block;
            width: 24px;
            height: 1px;
            margin: 16px auto 0;
            opacity: 0;
            animation: fadeIn 0.8s ease 0.4s forwards;
            transition: background-color 0.35s ease;
        }

        body.light .tagline { color: #b5b4b0; }
        body.dark  .tagline { color: #4a4a50; }
        body.light .accent  { background-color: #d0cfcb; }
        body.dark  .accent  { background-color: #2e2e33; }

        @keyframes enterUp {
            from { transform: translateY(12px); }
            to   { transform: translateY(0); }
        }

        @keyframes fadeIn {
            from { opacity: 0; }
            to   { opacity: 1; }
        }
    </style>
</head>
<body class="light">
    <div class="welcome">
        <div class="tagline">your blank slate</div>
        <div class="accent"></div>
    </div>
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
    return QSize(width, 44);
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

        // Tab background — clean pill shape with spacing
        painter.setPen(Qt::NoPen);
        QRect bgRect = tabRectArea.adjusted(3, 5, -3, -3);

        if (isSelected) {
            // Active tab: subtle filled background
            painter.setBrush(m_isDark ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 8));
            painter.drawRoundedRect(bgRect, 7, 7);

            // Bottom accent line — the key differentiator
            QRect accentRect(bgRect.left() + bgRect.width() / 2 - 8, bgRect.bottom() - 1, 16, 2);
            painter.setBrush(m_isDark ? QColor("#5ba3d9") : QColor("#5ba3d9"));
            painter.drawRoundedRect(accentRect, 1, 1);
        } else if (isHovered) {
            painter.setBrush(m_isDark ? QColor(255, 255, 255, 6) : QColor(0, 0, 0, 4));
            painter.drawRoundedRect(bgRect, 7, 7);
        }

        // Vertical center for alignment
        int centerY = bgRect.center().y();

        // Close button rect
        int closeSize = 14;
        int closeX = bgRect.right() - closeSize - 8;
        int closeY = centerY - closeSize / 2;
        QRect closeRect(closeX, closeY, closeSize, closeSize);

        // Favicon
        int textLeftOffset = 12;
        QIcon favicon = m_favicons.value(i, QIcon());
        if (!favicon.isNull()) {
            int iconSize = 14;
            int iconX = bgRect.left() + 10;
            int iconY = centerY - iconSize / 2;
            favicon.paint(&painter, iconX, iconY, iconSize, iconSize);
            textLeftOffset = 30;
        }

        // Tab text — refined typography
        QRect textRect(bgRect.left() + textLeftOffset, bgRect.top(),
                       closeRect.left() - bgRect.left() - textLeftOffset - 4, bgRect.height());

        QColor textColor;
        if (m_isDark) {
            textColor = isSelected ? QColor("#c8c8cd") : QColor("#67676d");
        } else {
            textColor = isSelected ? QColor("#2c2c28") : QColor("#999894");
        }

        QFont tabFont = font();
        tabFont.setWeight(isSelected ? QFont::Medium : QFont::Normal);
        painter.setFont(tabFont);
        painter.setPen(textColor);
        QString elidedText = painter.fontMetrics().elidedText(tabText(i), Qt::ElideRight, textRect.width());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);

        // Close button — only visible on hover or selected
        QPoint mousePos = mapFromGlobal(QCursor::pos());
        bool closeHovered = closeRect.contains(mousePos);
        bool showClose = isSelected || isHovered;

        if (showClose) {
            if (closeHovered) {
                painter.setBrush(m_isDark ? QColor(255, 255, 255, 15) : QColor(0, 0, 0, 8));
                painter.setPen(Qt::NoPen);
                painter.drawRoundedRect(closeRect.adjusted(-2, -2, 2, 2), 4, 4);
            }

            // X mark — thin, elegant lines
            QColor xColor = closeHovered
                ? (m_isDark ? QColor("#b0b0b5") : QColor("#555550"))
                : (m_isDark ? QColor("#5a5a60") : QColor("#b5b4b0"));
            painter.setPen(QPen(xColor, 1.2, Qt::SolidLine, Qt::RoundCap));
            int m = 4;
            painter.drawLine(closeRect.left() + m, closeRect.top() + m,
                             closeRect.right() - m, closeRect.bottom() - m);
            painter.drawLine(closeRect.right() - m, closeRect.top() + m,
                             closeRect.left() + m, closeRect.bottom() - m);
        }
    }
}

QRect SlateTabBar::closeButtonRect(int index) const
{
    QRect tabRectArea = tabRect(index);
    QRect bgRect = tabRectArea.adjusted(3, 5, -3, -3);
    int closeSize = 14;
    int closeX = bgRect.right() - closeSize - 8;
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

void SlateTabBar::reorderFavicons(int from, int to)
{
    QIcon movedIcon = m_favicons.take(from);

    // Shift indices between from and to
    QMap<int, QIcon> updated;
    for (auto it = m_favicons.begin(); it != m_favicons.end(); ++it) {
        int idx = it.key();
        if (from < to) {
            // Moved right: indices (from, to] shift left by 1
            if (idx > from && idx <= to)
                updated[idx - 1] = it.value();
            else
                updated[idx] = it.value();
        } else {
            // Moved left: indices [to, from) shift right by 1
            if (idx >= to && idx < from)
                updated[idx + 1] = it.value();
            else
                updated[idx] = it.value();
        }
    }
    updated[to] = movedIcon;
    m_favicons = updated;
    update();
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
    m_titleBar->setFixedHeight(48);

    m_titleBarLayout = new QHBoxLayout(m_titleBar);
    m_titleBarLayout->setContentsMargins(8, 0, 0, 0);
    m_titleBarLayout->setSpacing(2);

    // Tab bar
    m_tabBar = new SlateTabBar();

    // New tab button
    m_newTabButton = new QPushButton("+");
    m_newTabButton->setObjectName("newTabButton");
    m_newTabButton->setFixedSize(26, 26);
    m_newTabButton->setToolTip("New Tab (Ctrl+T)");
    m_newTabButton->setCursor(Qt::PointingHandCursor);

    // Spacer to push window controls to right
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Window controls — compact, refined
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

    m_titleBarLayout->addWidget(m_tabBar);
    m_titleBarLayout->addWidget(m_newTabButton);
    m_titleBarLayout->addWidget(spacer);
    m_titleBarLayout->addWidget(m_minimizeButton);
    m_titleBarLayout->addWidget(m_maximizeButton);
    m_titleBarLayout->addWidget(m_closeButton);

    // === NAVIGATION BAR ===
    m_navBar = new QWidget();
    m_navBar->setObjectName("navBar");
    m_navBar->setFixedHeight(46);

    m_navLayout = new QHBoxLayout(m_navBar);
    m_navLayout->setContentsMargins(12, 4, 12, 8);
    m_navLayout->setSpacing(4);

    // Back button
    m_backButton = new QPushButton();
    m_backButton->setObjectName("navButton");
    m_backButton->setText("<");
    m_backButton->setFixedSize(34, 34);
    m_backButton->setEnabled(false);
    m_backButton->setToolTip("Back");
    m_backButton->setCursor(Qt::PointingHandCursor);

    // Forward button
    m_forwardButton = new QPushButton();
    m_forwardButton->setObjectName("navButton");
    m_forwardButton->setText(">");
    m_forwardButton->setFixedSize(34, 34);
    m_forwardButton->setEnabled(false);
    m_forwardButton->setToolTip("Forward");
    m_forwardButton->setCursor(Qt::PointingHandCursor);

    // Reload button
    m_reloadButton = new QPushButton();
    m_reloadButton->setObjectName("navButton");
    m_reloadButton->setText("↻");
    m_reloadButton->setFixedSize(34, 34);
    m_reloadButton->setToolTip("Reload (Ctrl+R)");
    m_reloadButton->setCursor(Qt::PointingHandCursor);

    // Address bar
    m_addressBar = new AddressBar();

    // Theme toggle
    m_themeToggle = new ThemeToggle();

    m_navLayout->addWidget(m_backButton);
    m_navLayout->addWidget(m_forwardButton);
    m_navLayout->addWidget(m_reloadButton);
    m_navLayout->addSpacing(8);
    m_navLayout->addWidget(m_addressBar, 1);
    m_navLayout->addSpacing(8);
    m_navLayout->addWidget(m_themeToggle);

    // === PROGRESS BAR (Feature 3) ===
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
    m_stackedWidget->setCurrentWidget(m_placeholderWidget);

    // === PERMISSION TOAST (Feature 9) ===
    m_toastLabel = new QLabel(m_stackedWidget);
    m_toastLabel->setObjectName("permissionToast");
    m_toastLabel->setFixedHeight(32);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->hide();

    // === FIND BAR (Feature 1) ===
    setupFindBar();

    // === DOWNLOAD BAR (Feature 10) ===
    m_downloadBar = new QWidget();
    m_downloadBar->setObjectName("downloadBar");
    m_downloadBar->setFixedHeight(40);
    m_downloadBar->hide();

    QHBoxLayout *dlLayout = new QHBoxLayout(m_downloadBar);
    dlLayout->setContentsMargins(12, 4, 12, 4);
    dlLayout->setSpacing(8);

    m_downloadLabel = new QLabel();
    m_downloadLabel->setObjectName("downloadLabel");

    m_downloadProgress = new QProgressBar();
    m_downloadProgress->setObjectName("downloadProgress");
    m_downloadProgress->setFixedWidth(180);
    m_downloadProgress->setFixedHeight(4);
    m_downloadProgress->setTextVisible(false);
    m_downloadProgress->setRange(0, 100);

    m_downloadCloseBtn = new QPushButton("✕");
    m_downloadCloseBtn->setObjectName("findCloseButton");
    m_downloadCloseBtn->setFixedSize(28, 28);
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
    connect(m_tabBar, &QTabBar::tabMoved, this, &MainWindow::onTabMoved);

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

    // Inject scrollbar styling into every page
    QWebEngineScript scrollbarScript;
    scrollbarScript.setName("slate-scrollbar");
    scrollbarScript.setSourceCode(scrollbarInjectionScript(m_isDarkMode));
    scrollbarScript.setInjectionPoint(QWebEngineScript::DocumentReady);
    scrollbarScript.setWorldId(QWebEngineScript::MainWorld);
    scrollbarScript.setRunsOnSubFrames(true);
    m_profile->scripts()->insert(scrollbarScript);

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
    painter.setPen(QPen(QColor("#161619"), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(4, 4, 24, 24);
    painter.setFont(QFont("Segoe UI Variable", 14, QFont::Medium));
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
            /* === Foundation === */
            QMainWindow, #centralWidget { background-color: #161619; }

            /* === Title Bar === */
            #titleBar { background-color: #1e1e22; }

            /* === Tab Bar === */
            #slateTabBar {
                background-color: transparent;
                border: none;
                qproperty-drawBase: 0;
                font-family: "Segoe UI Variable", "Segoe UI", system-ui, sans-serif;
                font-size: 12px;
                font-weight: 400;
            }

            /* === New Tab Button === */
            #newTabButton {
                background-color: transparent;
                border: none;
                color: #5a5a60;
                font-family: "Segoe UI Variable", "Segoe UI", sans-serif;
                font-size: 17px;
                font-weight: 300;
                border-radius: 8px;
            }
            #newTabButton:hover {
                background-color: rgba(255, 255, 255, 0.06);
                color: #8a8a90;
            }

            /* === Window Controls === */
            #windowButton {
                background-color: transparent;
                border: none;
                color: #67676d;
                font-family: "Segoe UI Symbol", "Segoe UI", sans-serif;
                font-size: 13px;
                font-weight: 400;
            }
            #windowButton:hover {
                background-color: rgba(255, 255, 255, 0.08);
                color: #a0a0a5;
            }
            #closeButton {
                background-color: transparent;
                border: none;
                color: #67676d;
                font-family: "Segoe UI Symbol", "Segoe UI", sans-serif;
                font-size: 13px;
                font-weight: 400;
            }
            #closeButton:hover {
                background-color: #e81123;
                color: #ffffff;
            }

            /* === Navigation Bar === */
            #navBar { background-color: #161619; }

            /* === Navigation Buttons === */
            #navButton {
                background-color: transparent;
                border: none;
                color: #8a8a90;
                font-family: "Segoe UI", sans-serif;
                font-size: 16px;
                font-weight: 400;
                border-radius: 8px;
            }
            #navButton:hover {
                background-color: rgba(255, 255, 255, 0.06);
                color: #c8c8cd;
            }
            #navButton:pressed {
                background-color: rgba(255, 255, 255, 0.10);
            }
            #navButton:disabled { color: #36363b; }

            /* === Address Bar === */
            #addressBar {
                background-color: #26262b;
                border: 1px solid transparent;
                border-radius: 10px;
                color: #e8e8ed;
                padding: 0px 14px;
                min-height: 34px;
                font-family: "Cascadia Code", "SF Mono", "Consolas", monospace;
                font-size: 13px;
                selection-background-color: #264f78;
            }
            #addressBar:focus {
                background-color: #1e1e22;
                border: 1px solid #3a3a40;
            }
            #addressBar::placeholder { color: #5a5a60; }

            /* === Content Area === */
            #contentArea { background-color: #161619; }

            /* === Loading Bar === */
            #loadingBar {
                background-color: transparent;
                border: none;
                max-height: 2px;
            }
            #loadingBar::chunk { background-color: #5ba3d9; }

            /* === Find Bar === */
            #findBar {
                background-color: #1e1e22;
                border-top: 1px solid #2e2e33;
            }
            #findEdit {
                background-color: #26262b;
                border: 1px solid #36363b;
                border-radius: 8px;
                color: #e8e8ed;
                padding: 0px 12px;
                font-family: "Segoe UI Variable", "Segoe UI", sans-serif;
                font-size: 13px;
            }
            #findEdit:focus { border-color: #5ba3d9; }
            #findLabel {
                color: #5a5a60;
                font-family: "Segoe UI Variable", "Segoe UI", sans-serif;
                font-size: 12px;
            }
            #findButton {
                background-color: transparent;
                border: none;
                border-radius: 6px;
                color: #8a8a90;
                font-size: 11px;
            }
            #findButton:hover { background-color: rgba(255, 255, 255, 0.06); }
            #findCloseButton {
                background-color: transparent;
                border: none;
                border-radius: 6px;
                color: #5a5a60;
                font-size: 12px;
            }
            #findCloseButton:hover {
                background-color: rgba(255, 255, 255, 0.06);
                color: #c8c8cd;
            }

            /* === Download Bar === */
            #downloadBar {
                background-color: #1e1e22;
                border-top: 1px solid #2e2e33;
            }
            #downloadLabel {
                color: #e8e8ed;
                font-family: "Segoe UI Variable", "Segoe UI", sans-serif;
                font-size: 12px;
            }
            #downloadProgress {
                background-color: #2e2e33;
                border: none;
                border-radius: 2px;
            }
            #downloadProgress::chunk {
                background-color: #5ba3d9;
                border-radius: 2px;
            }

            /* === Permission Toast === */
            #permissionToast {
                background-color: #1e1e22;
                color: #e8e8ed;
                border-bottom: 1px solid #2e2e33;
                font-family: "Segoe UI Variable", "Segoe UI", sans-serif;
                font-size: 12px;
                padding: 0px 16px;
            }

            /* === Tooltip === */
            QToolTip {
                background-color: #e8e8ed;
                color: #161619;
                border: none;
                padding: 4px 8px;
                font-family: "Segoe UI Variable", "Segoe UI", sans-serif;
                font-size: 12px;
                border-radius: 6px;
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

    // Update scrollbar script in profile (for future pages)
    if (m_profile) {
        for (const QWebEngineScript &s : m_profile->scripts()->find("slate-scrollbar"))
            m_profile->scripts()->remove(s);
        QWebEngineScript scrollbarScript;
        scrollbarScript.setName("slate-scrollbar");
        scrollbarScript.setSourceCode(scrollbarInjectionScript(isDark));
        scrollbarScript.setInjectionPoint(QWebEngineScript::DocumentReady);
        scrollbarScript.setWorldId(QWebEngineScript::MainWorld);
        scrollbarScript.setRunsOnSubFrames(true);
        m_profile->scripts()->insert(scrollbarScript);
    }

    // Update existing tabs via JavaScript
    QString scrollbarJs = scrollbarInjectionScript(isDark);
    for (int i = 0; i < m_stackedWidget->count(); ++i) {
        QWebEngineView *view = qobject_cast<QWebEngineView*>(m_stackedWidget->widget(i));
        if (view) {
            // Update scrollbar in all tabs
            view->page()->runJavaScript(scrollbarJs);

            // Update welcome page theme
            QString url = view->url().toString();
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
            <!DOCTYPE html><html><head>
            <link rel="preconnect" href="https://fonts.googleapis.com">
            <link href="https://fonts.googleapis.com/css2?family=DM+Sans:ital,opsz,wght@0,9..40,100..1000;1,9..40,100..1000&display=swap" rel="stylesheet">
            <style>
            *, *::before, *::after { margin: 0; padding: 0; box-sizing: border-box; }
            body {
                font-family: 'DM Sans', -apple-system, 'Segoe UI', system-ui, sans-serif;
                display: flex; align-items: center; justify-content: center;
                height: 100vh; overflow: hidden;
                background-color: %1; color: %2;
            }
            .container {
                text-align: center; max-width: 440px; padding: 40px;
                animation: enterUp 0.6s cubic-bezier(0.16, 1, 0.3, 1) both;
            }
            .shield {
                width: 48px; height: 48px; margin: 0 auto 24px;
                border: 2px solid %5; border-radius: 12px;
                display: flex; align-items: center; justify-content: center;
                font-size: 22px; color: %5;
                opacity: 0; animation: fadeIn 0.8s ease 0.1s forwards;
            }
            h1 {
                font-size: 18px; font-weight: 500; letter-spacing: 0.5px;
                margin-bottom: 12px;
                opacity: 0; animation: fadeIn 0.8s ease 0.2s forwards;
            }
            p {
                font-size: 13px; color: %3; line-height: 1.7;
                opacity: 0; animation: fadeIn 0.8s ease 0.3s forwards;
            }
            .url {
                font-family: 'Cascadia Code', 'SF Mono', Consolas, monospace;
                font-size: 12px; color: %5;
                background: %4; padding: 6px 14px; border-radius: 8px;
                margin-top: 20px; display: inline-block; letter-spacing: 0.3px;
                opacity: 0; animation: fadeIn 0.8s ease 0.4s forwards;
            }
            @keyframes enterUp {
                from { transform: translateY(12px); }
                to   { transform: translateY(0); }
            }
            @keyframes fadeIn {
                from { opacity: 0; }
                to   { opacity: 1; }
            }
            </style></head><body>
            <div class="container">
                <div class="shield">⚠</div>
                <h1>Connection not secure</h1>
                <p>Slate blocked this connection because the site's certificate is not trusted. This protects your data from being intercepted.</p>
                <div class="url">%6</div>
            </div></body></html>
        )").arg(
            m_isDarkMode ? "#161619" : "#fafaf9",   // bg
            m_isDarkMode ? "#c8c8cd" : "#2c2c28",   // text
            m_isDarkMode ? "#5a5a60" : "#999894",    // muted text
            m_isDarkMode ? "#26262b" : "#ededeb",    // url bg
            m_isDarkMode ? "#d4976a" : "#c47a4a",    // warning accent
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
    view->page()->setBackgroundColor(m_isDarkMode ? QColor("#161619") : QColor("#fafaf9"));

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

void MainWindow::onTabMoved(int from, int to)
{
    // Sync QStackedWidget order with tab bar after drag-reorder
    QWidget *widget = m_stackedWidget->widget(from);
    m_stackedWidget->removeWidget(widget);
    m_stackedWidget->insertWidget(to, widget);
    m_stackedWidget->setCurrentIndex(to);

    // Sync favicon map
    m_tabBar->reorderFavicons(from, to);
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
            <!DOCTYPE html><html><head>
            <link rel="preconnect" href="https://fonts.googleapis.com">
            <link href="https://fonts.googleapis.com/css2?family=DM+Sans:ital,opsz,wght@0,9..40,100..1000;1,9..40,100..1000&display=swap" rel="stylesheet">
            <style>
            *, *::before, *::after { margin: 0; padding: 0; box-sizing: border-box; }

            body {
                font-family: 'DM Sans', -apple-system, 'Segoe UI', system-ui, sans-serif;
                display: flex; align-items: center; justify-content: center;
                height: 100vh; overflow: hidden;
                background-color: %1; color: %2;
            }

            .container {
                text-align: center; max-width: 520px; padding: 40px;
                animation: enterUp 0.6s cubic-bezier(0.16, 1, 0.3, 1) both;
            }

            h1 {
                font-size: 28px; font-weight: 300; letter-spacing: 3px;
                text-transform: lowercase; margin-bottom: 6px;
                opacity: 0; animation: fadeIn 0.8s ease 0.1s forwards;
            }

            .accent-line {
                display: block; width: 24px; height: 1px;
                margin: 12px auto 0; background-color: %3;
                opacity: 0; animation: fadeIn 0.8s ease 0.25s forwards;
            }

            .version {
                font-size: 12px; color: %4; margin-top: 12px; letter-spacing: 0.5px;
                opacity: 0; animation: fadeIn 0.8s ease 0.35s forwards;
            }

            .features {
                text-align: left; margin: 28px auto 0; max-width: 380px;
                opacity: 0; animation: fadeIn 0.8s ease 0.45s forwards;
            }

            .features h3 {
                font-size: 10px; font-weight: 500; color: %4;
                text-transform: uppercase; letter-spacing: 2px;
                margin: 24px 0 10px; padding-bottom: 6px;
                border-bottom: 1px solid %3;
            }

            .features .item {
                font-size: 13px; color: %5; padding: 5px 0;
                display: flex; justify-content: space-between; align-items: center;
            }

            .features .key {
                font-family: 'Cascadia Code', 'SF Mono', Consolas, monospace;
                font-size: 11px; background: %6; padding: 2px 8px;
                border-radius: 4px; color: %4; letter-spacing: 0.3px;
            }

            @keyframes enterUp {
                from { transform: translateY(12px); }
                to   { transform: translateY(0); }
            }
            @keyframes fadeIn {
                from { opacity: 0; }
                to   { opacity: 1; }
            }
            </style></head><body>
            <div class="container">
                <h1>slate</h1>
                <div class="accent-line"></div>
                <div class="version">v%7 — a clean slate for web browsing</div>
                <div class="features">
                    <h3>shortcuts</h3>
                    <div class="item"><span>New Tab</span><span class="key">Ctrl+T</span></div>
                    <div class="item"><span>Close Tab</span><span class="key">Ctrl+W</span></div>
                    <div class="item"><span>Focus Address Bar</span><span class="key">Ctrl+L</span></div>
                    <div class="item"><span>Reload</span><span class="key">Ctrl+R</span></div>
                    <div class="item"><span>Find in Page</span><span class="key">Ctrl+F</span></div>
                    <div class="item"><span>Zoom In / Out / Reset</span><span class="key">Ctrl +/- 0</span></div>
                    <div class="item"><span>Next / Prev Tab</span><span class="key">Ctrl+Tab</span></div>
                    <div class="item"><span>Fullscreen</span><span class="key">F11</span></div>
                    <div class="item"><span>Print to PDF</span><span class="key">Ctrl+P</span></div>
                    <div class="item"><span>Developer Tools</span><span class="key">F12</span></div>
                    <h3>privacy</h3>
                    <div class="item"><span>HTTPS-only mode</span><span class="key">always on</span></div>
                    <div class="item"><span>Ad & tracker blocking</span><span class="key">always on</span></div>
                    <div class="item"><span>No history saved</span><span class="key">by design</span></div>
                    <div class="item"><span>No cookies persist</span><span class="key">by design</span></div>
                </div>
            </div></body></html>
        )").arg(
            m_isDarkMode ? "#161619" : "#fafaf9",   // bg
            m_isDarkMode ? "#c8c8cd" : "#2c2c28",   // text primary
            m_isDarkMode ? "#2e2e33" : "#d0cfcb",   // accent-line / borders
            m_isDarkMode ? "#5a5a60" : "#999894",    // muted text
            m_isDarkMode ? "#8a8a90" : "#65645f",    // item text
            m_isDarkMode ? "#26262b" : "#ededeb",    // key bg
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
