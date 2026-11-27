/**
 * Slate Browser
 * RequestFilter.cpp - Per-session request interceptor
 */

#include "RequestFilter.h"
#include "Blocklist.h"
#include "UrlInput.h"

RequestFilter::RequestFilter(const Blocklist *blocklist, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent), m_blocklist(blocklist)
{
}

void RequestFilter::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    const QUrl url = info.requestUrl();

    if (url.scheme() == "http" && !isLocalNetworkHost(url.host())) {
        QUrl httpsUrl = url;
        httpsUrl.setScheme("https");
        info.redirect(httpsUrl);
        return;
    }

    // Never block a page the user navigated to, only what pages pull in.
    // A listed site the user chose to visit also loads its own resources.
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame
        || m_blocklist->contains(info.firstPartyUrl().host())) {
        return;
    }

    if (m_blocklist->contains(url.host())) {
        info.block(true);
        m_blockedCount++;
        emit blockedCountChanged();
    }
}
