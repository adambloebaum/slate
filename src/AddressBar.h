/**
 * Slate Browser - A Clean Slate for Web Browsing
 * AddressBar.h - Minimal address bar header
 */

#ifndef ADDRESSBAR_H
#define ADDRESSBAR_H

#include <QLineEdit>
#include <QTimer>

class AddressBar : public QLineEdit
{
    Q_OBJECT

public:
    explicit AddressBar(QWidget *parent = nullptr);

signals:
    void urlEntered(const QString &url);
    void focusGained();
    void focusLost();

protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // ADDRESSBAR_H
