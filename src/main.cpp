#include "mainwindow.h"
#include <QApplication>

const QStringList commands = {
    "exportmapimage",
};

const bool USE_CLI = false; // TODO: Remove

QCoreApplication* createApplication(int &argc, char *argv[])
{
    // Search for commands to determine which application to run.
    // QCommandLineParser requires an application to already exist, so we don't
    // use it here to avoid a potentially wasteful QCoreApplication constructor.
    // TODO

    if (USE_CLI) {
        // Create Porymap CLI application
        return new QCoreApplication(argc, argv);
    }

    // Create Porymap GUI application
    QApplication * app = new QApplication(argc, argv);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);
    QApplication::setApplicationDisplayName("porymap");
    QApplication::setWindowIcon(QIcon(":/icons/porymap-icon-2.ico"));
    app->setStyle("fusion");
    return app;
}

int main(int argc, char* argv[])
{
    QScopedPointer<MainWindow> w;

    // Initialize application
    QScopedPointer<QCoreApplication> app(createApplication(argc, argv));
    QCoreApplication::setOrganizationName("pret");
    QCoreApplication::setApplicationName("porymap");
    //QCoreApplication::setApplicationVersion("5.2.0");

    // Set up argument parser
    QCommandLineParser parser;
    parser.setApplicationDescription("Test helper");
    parser.addHelpOption();
    //parser.addVersionOption();

    QStringList args;

    parser.process(*app);

    if (USE_CLI) {
        // Init Porymap CLI application
    } else {
        // Init Porymap GUI application
        w.reset(new MainWindow(nullptr));
        w->show();
    }

    return app->exec();
}
