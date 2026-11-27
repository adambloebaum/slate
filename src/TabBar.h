/**
 * Slate Browser
 * TabBar.h - Custom-painted tab bar
 */

#ifndef TABBAR_H
#define TABBAR_H

#include <QTabBar>

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

#endif // TABBAR_H
