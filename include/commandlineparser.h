#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include "project.h"
#include <QCommandLineParser>

class CommandLineParser : public QCommandLineParser
{
public:
    CommandLineParser();

public:
    void showMessage(const QString &text);
    void showError(const QString &text);
    void showError();
    void showHelp(int errCode = 0);
    void addSilentOption();
    void addCommandArgument(const QString &name, const QString &description, const QList<QCommandLineOption> &options);
    bool checkExclusiveOptions(const QCommandLineOption &a, const QCommandLineOption &b);
    void process(const QStringList &arguments);

private:
    bool silent = false; // TODO: Should this be applied for the log functions too?

};

#endif // COMMANDLINEPARSER_H
