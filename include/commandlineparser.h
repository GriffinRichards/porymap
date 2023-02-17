#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include "project.h"
#include <QCommandLineParser>

class CommandLineParser : public QCommandLineParser
{
public:
    CommandLineParser();

public:
    void showError(const QString &text);
    void showError();
    void showHelp(int errCode = 0);
    void addCommandArgument(const QString &name, const QString &description, const QList<QCommandLineOption> &options);
    bool checkExclusiveOptions(const QCommandLineOption &a, const QCommandLineOption &b);

};

#endif // COMMANDLINEPARSER_H
