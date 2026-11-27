/**
 * Slate Browser
 * UrlInput.h - Turn address bar text into a URL
 */

#ifndef URLINPUT_H
#define URLINPUT_H

#include <QString>
#include <QUrl>

// True for localhost, loopback, private-range, and link-local hosts.
// These are exempt from the HTTPS-only upgrade (dev servers, router pages).
bool isLocalNetworkHost(const QString &host);

// Address bar text -> URL to load. Anything that does not look like a
// host becomes a DuckDuckGo search.
QUrl urlFromInput(const QString &input);

#endif // URLINPUT_H
