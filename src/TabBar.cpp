/**
 * Slate Browser
 * TabBar.cpp - Custom-painted tab bar
 */

#include "TabBar.h"
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionTab>

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
        QIcon favicon = tabIcon(i);
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

void SlateTabBar::mouseMoveEvent(QMouseEvent *event)
{
    update();  // Repaint for hover effects
    QTabBar::mouseMoveEvent(event);
}
