/**
 * Slate Browser
 * UrlInput.cpp - Turn address bar text into a URL
 */

#include "UrlInput.h"
#include <QHostAddress>

bool isLocalNetworkHost(const QString &host)
{
    if (host == "localhost" || host.endsWith(".localhost") || host.endsWith(".local")) {
        return true;
    }
    const QHostAddress address(host);
    return address.isLoopback() || address.isPrivateUse() || address.isLinkLocal();
}

QUrl urlFromInput(const QString &input)
{
    const QString text = input.trimmed();

    // Explicit scheme (https://, http://, file://): take it as typed
    const int schemeEnd = text.indexOf("://");
    if (schemeEnd > 0 && !text.contains(' ')) {
        return QUrl(text);
    }

    QUrl url("https://" + text);
    const QString host = url.host();
    const bool looksLikeHost = host.contains('.') || host.contains(':') || host == "localhost";
    if (text.contains(' ') || !url.isValid() || !looksLikeHost) {
        return QUrl("https://duckduckgo.com/?q=" + QString::fromLatin1(QUrl::toPercentEncoding(text)));
    }

    if (isLocalNetworkHost(host)) {
        url.setScheme("http");
    }
    return url;
}
