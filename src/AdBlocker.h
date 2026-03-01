/**
 * Slate Browser - A Clean Slate for Web Browsing
 * AdBlocker.h - Simple native ad blocking
 */

#ifndef ADBLOCKER_H
#define ADBLOCKER_H

#include <QWebEngineUrlRequestInterceptor>
#include <QSet>
#include <QString>
#include <QStringList>

class AdBlocker : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    explicit AdBlocker(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

    int blockedCount() const { return m_blockedCount; }
    int domainCount() const { return m_blockedDomains.size(); }

    // Load additional domains from a hosts-file format (one domain per line, or "0.0.0.0 domain")
    int loadHostsFile(const QString &filePath);

private:
    bool shouldBlock(const QUrl &url) const;
    QSet<QString> m_blockedDomains;
    QStringList m_blockedUrlPatterns;
    int m_blockedCount = 0;
};

#endif // ADBLOCKER_H
