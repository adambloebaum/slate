/**
 * Slate Browser
 * RequestFilter.h - Per-session request interceptor
 */

#ifndef REQUESTFILTER_H
#define REQUESTFILTER_H

#include <QWebEngineUrlRequestInterceptor>

class Blocklist;

// One per session (profile). Upgrades http to https outside the local
// network and blocks subresource requests to listed hosts.
class RequestFilter : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    RequestFilter(const Blocklist *blocklist, QObject *parent);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    int blockedCount() const { return m_blockedCount; }

signals:
    void blockedCountChanged();

private:
    const Blocklist *m_blocklist;
    int m_blockedCount = 0;
};

#endif // REQUESTFILTER_H
