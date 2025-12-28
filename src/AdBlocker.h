/**
 * Slate Browser - A Clean Slate for Web Browsing
 * AdBlocker.h - Simple native ad blocking
 */

#ifndef ADBLOCKER_H
#define ADBLOCKER_H

#include <QWebEngineUrlRequestInterceptor>
#include <QSet>
#include <QString>

class AdBlocker : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    explicit AdBlocker(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

    int blockedCount() const { return m_blockedCount; }

private:
    bool shouldBlock(const QUrl &url) const;
    QSet<QString> m_blockedDomains;
    int m_blockedCount = 0;
};

#endif // ADBLOCKER_H
