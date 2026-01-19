/**
 * Slate Browser - A Clean Slate for Web Browsing
 * AddressBar.cpp - Minimal address bar implementation
 */

#include "AddressBar.h"
#include <QFocusEvent>
#include <QKeyEvent>

AddressBar::AddressBar(QWidget *parent)
    : QLineEdit(parent)
{
    setObjectName("addressBar");
    setPlaceholderText("Search or enter URL");
    setClearButtonEnabled(false);  // No X button

    connect(this, &QLineEdit::returnPressed, [this]() {
        emit urlEntered(text());
    });
}

void AddressBar::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    emit focusGained();
    // Select all text when focused
    QTimer::singleShot(0, this, &QLineEdit::selectAll);
}

void AddressBar::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
    emit focusLost();
}

void AddressBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        clearFocus();
        return;
    }
    QLineEdit::keyPressEvent(event);
}
