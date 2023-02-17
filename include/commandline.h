#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include "project.h"
#include "commandlineparser.h"

struct Command {
    QString description;
    QList<QCommandLineOption> options;
};

class CommandLine : public QObject
{
    Q_OBJECT
public:
    CommandLine(QObject *parent = nullptr) : QObject(parent) {};
    ~CommandLine();

public:
    static bool isCommand(QString s);
    bool parse();
    void run();

private:
    bool loadProject();

    void runExportImage();

private:
    CommandLineParser parser;
    Project * project = nullptr;
    QString commandName;
    QStringList args;
};

#endif // COMMANDLINE_H
