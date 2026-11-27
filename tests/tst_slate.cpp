/**
 * Slate Browser
 * tst_slate.cpp - Tests for address bar parsing and the blocklist
 */

#include <QTemporaryFile>
#include <QTest>
#include "Blocklist.h"
#include "UrlInput.h"

class TestSlate : public QObject
{
    Q_OBJECT

private slots:
    void urlFromInput_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        QTest::newRow("bare domain") << "example.com" << "https://example.com";
        QTest::newRow("domain with path") << "example.com/a?b=c" << "https://example.com/a?b=c";
        QTest::newRow("whitespace") << "  example.com  " << "https://example.com";
        QTest::newRow("explicit https") << "https://example.com/x" << "https://example.com/x";
        QTest::newRow("explicit http is kept for the filter") << "http://example.com" << "http://example.com";
        QTest::newRow("file url") << "file:///C:/notes.html" << "file:///C:/notes.html";
        QTest::newRow("localhost with port") << "localhost:3000" << "http://localhost:3000";
        QTest::newRow("loopback") << "127.0.0.1:8080/api" << "http://127.0.0.1:8080/api";
        QTest::newRow("router") << "192.168.1.1" << "http://192.168.1.1";
        QTest::newRow("public ip") << "8.8.8.8" << "https://8.8.8.8";
        QTest::newRow("single word") << "weather" << "https://duckduckgo.com/?q=weather";
        QTest::newRow("phrase") << "what is node.js" << "https://duckduckgo.com/?q=what%20is%20node.js";
        QTest::newRow("phrase with symbols") << "c++ & rust" << "https://duckduckgo.com/?q=c%2B%2B%20%26%20rust";
    }

    void urlFromInput()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(::urlFromInput(input).toString(QUrl::FullyEncoded), expected);
    }

    void localNetworkHosts()
    {
        QVERIFY(isLocalNetworkHost("localhost"));
        QVERIFY(isLocalNetworkHost("app.localhost"));
        QVERIFY(isLocalNetworkHost("printer.local"));
        QVERIFY(isLocalNetworkHost("127.0.0.1"));
        QVERIFY(isLocalNetworkHost("::1"));
        QVERIFY(isLocalNetworkHost("10.0.0.5"));
        QVERIFY(isLocalNetworkHost("192.168.1.1"));
        QVERIFY(!isLocalNetworkHost("example.com"));
        QVERIFY(!isLocalNetworkHost("8.8.8.8"));
        QVERIFY(!isLocalNetworkHost("notlocalhost.com"));
    }

    void builtinListMatchesSubdomainsOnly()
    {
        Blocklist list;
        QVERIFY(list.contains("doubleclick.net"));
        QVERIFY(list.contains("ad.doubleclick.net"));
        QVERIFY(list.contains("a.b.doubleclick.net"));
        QVERIFY(!list.contains("notdoubleclick.net"));
        QVERIFY(!list.contains("doubleclick.net.example.com"));
        QVERIFY(!list.contains("github.com"));
        QVERIFY(!list.contains(""));
    }

    void loadsHostsFormat()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write("# Title: test list\n"
                   "\n"
                   "127.0.0.1 localhost\n"
                   "255.255.255.255 broadcasthost\n"
                   "0.0.0.0 0.0.0.0\n"
                   "0.0.0.0 Ads.Example.com\n"
                   "127.0.0.1\ttracker.example.org  # inline comment\n"
                   "plain.example.net\n"
                   "0.0.0.0 ads.example.com\n"
                   "not a hosts line\n");
        file.close();

        Blocklist list;
        const int before = list.size();
        QCOMPARE(list.loadHostsFile(file.fileName()), 3);
        QCOMPARE(list.size(), before + 3);

        QVERIFY(list.contains("ads.example.com"));
        QVERIFY(list.contains("x.tracker.example.org"));
        QVERIFY(list.contains("plain.example.net"));
        QVERIFY(!list.contains("example.com"));
        QVERIFY(!list.contains("localhost"));
        QVERIFY(!list.contains("0.0.0.0"));
    }

    void missingFileAddsNothing()
    {
        Blocklist list;
        QCOMPARE(list.loadHostsFile("does-not-exist.txt"), 0);
    }
};

QTEST_GUILESS_MAIN(TestSlate)
#include "tst_slate.moc"
