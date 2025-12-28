/**
 * Slate Browser - A Clean Slate for Web Browsing
 * AdBlocker.cpp - Simple native ad blocking
 */

#include "AdBlocker.h"
#include <QUrl>

AdBlocker::AdBlocker(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
    // Common ad and tracking domains
    m_blockedDomains = {
        // Google Ads
        "doubleclick.net",
        "googlesyndication.com",
        "googleadservices.com",
        "google-analytics.com",
        "googletagmanager.com",
        "googletagservices.com",
        "pagead2.googlesyndication.com",
        "adservice.google.com",

        // Facebook/Meta
        "facebook.com/tr",
        "connect.facebook.net",
        "pixel.facebook.com",

        // Amazon Ads
        "amazon-adsystem.com",
        "aax.amazon-adsystem.com",

        // Common ad networks
        "adnxs.com",
        "adsrvr.org",
        "adform.net",
        "admob.com",
        "advertising.com",
        "bidswitch.net",
        "casalemedia.com",
        "criteo.com",
        "criteo.net",
        "demdex.net",
        "dotomi.com",
        "exelator.com",
        "eyeota.net",
        "flashtalking.com",
        "imrworldwide.com",
        "indexww.com",
        "lijit.com",
        "mathtag.com",
        "mediamath.com",
        "moatads.com",
        "mookie1.com",
        "openx.net",
        "outbrain.com",
        "pubmatic.com",
        "quantserve.com",
        "rfihub.com",
        "richrelevance.com",
        "rlcdn.com",
        "rubiconproject.com",
        "sascdn.com",
        "scorecardresearch.com",
        "sharethrough.com",
        "simpli.fi",
        "sitescout.com",
        "smartadserver.com",
        "spotxchange.com",
        "steelhousemedia.com",
        "taboola.com",
        "tapad.com",
        "tidaltv.com",
        "tremorhub.com",
        "tribalfusion.com",
        "turn.com",
        "undertone.com",
        "yieldmo.com",
        "zedo.com",

        // Tracking
        "bluekai.com",
        "bkrtx.com",
        "bounceexchange.com",
        "branch.io",
        "brealtime.com",
        "chartbeat.com",
        "clicktale.net",
        "conviva.com",
        "crwdcntrl.net",
        "districtm.io",
        "dmtry.com",
        "doubleverify.com",
        "everesttech.net",
        "hotjar.com",
        "hs-analytics.net",
        "hsadspixel.net",
        "krxd.net",
        "liveramp.com",
        "liveintent.com",
        "mixpanel.com",
        "mxpnl.com",
        "newrelic.com",
        "nr-data.net",
        "omtrdc.net",
        "optimizely.com",
        "pardot.com",
        "parsely.com",
        "segment.com",
        "segment.io",
        "tagging.nfl.com",
        "trustarc.com",
        "truste.com",
        "yadro.ru",

        // Pop-ups / Malware
        "propellerads.com",
        "popcash.net",
        "popads.net",
        "revcontent.com",
        "mgid.com",
        "contentabc.com",
    };
}

bool AdBlocker::shouldBlock(const QUrl &url) const
{
    QString host = url.host().toLower();

    // Check exact match and parent domains
    while (!host.isEmpty()) {
        if (m_blockedDomains.contains(host)) {
            return true;
        }
        // Remove first subdomain and check again
        int dot = host.indexOf('.');
        if (dot == -1) break;
        host = host.mid(dot + 1);
    }

    // Check URL path for common ad patterns
    QString path = url.path().toLower();
    if (path.contains("/ads/") ||
        path.contains("/ad/") ||
        path.contains("/advert") ||
        path.contains("/banner") ||
        path.contains("/sponsor")) {
        return true;
    }

    return false;
}

void AdBlocker::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (shouldBlock(info.requestUrl())) {
        info.block(true);
        m_blockedCount++;
    }
}
