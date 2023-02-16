#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include "project.h"

#include <QCommandLineParser>

struct Command {
    QString description;
    QList<QCommandLineOption> options;
};

class CommandLine : public QObject
{
    Q_OBJECT
public:
    CommandLine(QWidget *parent = nullptr);
    ~CommandLine();

public:
    static bool isCommand(QString s);
    bool parse();
    void run();

private:
    QCommandLineParser parser;
    Project * project = nullptr;
    QString commandName;
    QStringList args;
    QHash<QString, QString> optionValues;
};

#endif // COMMANDLINE_H
