/**
 * Slate Browser
 * Blocklist.cpp - Set of ad and tracker hosts
 */

#include "Blocklist.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>

// Unified hosts list (ads + malware), MIT licensed, updated daily upstream
static const char *REMOTE_LIST_URL = "https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts";
static const int REFRESH_DAYS = 7;

// Baseline used until the remote list has been downloaded once
static const char *BUILTIN_DOMAINS[] = {
    "doubleclick.net",
    "googlesyndication.com",
    "googleadservices.com",
    "google-analytics.com",
    "googletagmanager.com",
    "googletagservices.com",
    "pagead2.googlesyndication.com",
    "adservice.google.com",
    "connect.facebook.net",
    "pixel.facebook.com",
    "amazon-adsystem.com",
    "aax.amazon-adsystem.com",
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
    "propellerads.com",
    "popcash.net",
    "popads.net",
    "revcontent.com",
    "mgid.com",
    "contentabc.com",
};

static QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/hosts.txt";
}

Blocklist::Blocklist(QObject *parent)
    : QObject(parent)
{
    for (const char *domain : BUILTIN_DOMAINS) {
        m_domains.insert(domain);
    }
}

bool Blocklist::contains(const QString &host) const
{
    // Check the host, then each parent domain
    QStringView rest = host;
    while (true) {
        if (m_domains.contains(rest.toString())) {
            return true;
        }
        const qsizetype dot = rest.indexOf('.');
        if (dot == -1) {
            return false;
        }
        rest = rest.mid(dot + 1);
    }
}

int Blocklist::loadHostsFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    int added = 0;
    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        const qsizetype comment = line.indexOf('#');
        if (comment != -1) {
            line.truncate(comment);
        }

        // "domain" or "0.0.0.0 domain": the domain is the last field
        const QList<QByteArray> fields = line.simplified().split(' ');
        const QString domain = QString::fromLatin1(fields.last()).toLower();
        if (fields.size() > 2 || !domain.contains('.') || domain == "0.0.0.0") {
            continue;
        }
        if (!m_domains.contains(domain)) {
            m_domains.insert(domain);
            added++;
        }
    }
    return added;
}

void Blocklist::syncRemoteList()
{
    const QFileInfo cache(cachePath());
    loadHostsFile(cache.filePath());
    if (cache.exists() && cache.lastModified().daysTo(QDateTime::currentDateTime()) < REFRESH_DAYS) {
        return;
    }

    auto *network = new QNetworkAccessManager(this);
    QNetworkReply *reply = network->get(QNetworkRequest(QUrl(REMOTE_LIST_URL)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, network]() {
        reply->deleteLater();
        network->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning("Blocklist download failed: %s", qPrintable(reply->errorString()));
            return;
        }
        QDir().mkpath(QFileInfo(cachePath()).path());
        QSaveFile file(cachePath());
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning("Cannot write blocklist cache: %s", qPrintable(file.errorString()));
            return;
        }
        file.write(reply->readAll());
        file.commit();
        loadHostsFile(cachePath());
    });
}
