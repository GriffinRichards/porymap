#include "mainwindow.h"
#include "commandline.h"
#include <QApplication>

QApplication* createGUIApplication(int &argc, char *argv[])
{
    QApplication * a = new QApplication(argc, argv);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);
    QApplication::setWindowIcon(QIcon(":/icons/porymap-icon-2.ico"));
    a->setStyle("fusion");
    return a;
}

int main(int argc, char* argv[])
{
    // QApplication is more expensive to create than a regular QCoreApplication, and is only necessary for the GUI.
    // If we recognize any Porymap commands in the args then use the latter automatically.
    bool skipGUI = false;
    CommandLine cli(nullptr);
    for (int i = 1; i < argc; i++) {
        if (cli.isCommand(argv[i])) {
            skipGUI = true;
            break;
        }
    }

    // Initialize application
    QScopedPointer<QCoreApplication> app(skipGUI ? new QCoreApplication(argc, argv) : createGUIApplication(argc, argv));
    QCoreApplication::setOrganizationName("pret");
    QCoreApplication::setApplicationName("porymap");
    QCoreApplication::setApplicationVersion(PORYMAP_VERSION);

    if (cli.parse()) {
        // Run CLI application
        cli.run();
        return app->exec();
    }

    // Run GUI application
    MainWindow w(nullptr);
    w.show();

    return app->exec();
}
