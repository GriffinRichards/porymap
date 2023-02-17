#include "commandlineparser.h"

CommandLineParser::CommandLineParser() :
    QCommandLineParser()
{
    this->setApplicationDescription("A map editor for the Pokémon generation 3 decompilation projects pokeruby, pokeemerald, and pokefirered.");
}

void CommandLineParser::showError(const QString &text) {
    fputs(qPrintable(text + "\n\n"), stderr);
    this->showHelp(1);
}

void CommandLineParser::showError() {
    this->showError(this->errorText());
}

// TODO: The help text is frustratingly bad (there is no way to e.g. remove the [options] that gets appended after ./porymap)
//       There is also no easy way to explicitly list the options in usage, or append text (like command descriptions) after the help text.
//       Perhaps this should be reimplemented.
void CommandLineParser::showHelp(int errCode) {
    QCommandLineParser::showHelp(errCode);
}

void CommandLineParser::addCommandArgument(const QString &name, const QString &description, const QList<QCommandLineOption> &options) {
    // TODO: Explicitly list each option name
    this->addPositionalArgument(name, description, QString("%1 [TODO]").arg(name));
    this->addOptions(options);
}

bool CommandLineParser::checkExclusiveOptions(const QCommandLineOption &a, const QCommandLineOption &b) {
    if (this->isSet(a) && this->isSet(b)) {
        this->showError(QString("Error: Cannot specify '%1' and '%2' together")
                            .arg(a.names().at(0))
                            .arg(b.names().at(0)));
        return true;
    }
    return false;

}
