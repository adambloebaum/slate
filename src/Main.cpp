/**
 * Slate Browser
 * Main.cpp - Application entry point
 */

#include <QApplication>
#include "MainWindow.h"
#include "Version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("Slate");
    QApplication::setOrganizationName("Slate");
    QApplication::setApplicationVersion(SLATE_VERSION);

    MainWindow window;
    window.show();

    return app.exec();
}
