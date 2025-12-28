/**
 * Slate Browser - A Clean Slate for Web Browsing
 * Main.cpp - Application entry point
 */

#include <QApplication>
#include <QtWebEngineQuick>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    // Initialize WebEngine before QApplication (required for Chromium)
    QtWebEngineQuick::initialize();

    QApplication app(argc, argv);

    // Application metadata
    QApplication::setApplicationName("Slate");
    QApplication::setOrganizationName("Slate");
    QApplication::setApplicationVersion("0.1.0");

    MainWindow window;
    window.show();

    return app.exec();
}
