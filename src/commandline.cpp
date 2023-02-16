#include "commandline.h"
#include "log.h"

const Command exportimage = {
    "Export a map or tileset image",
    {
        {{"o", "output"}, "Output file path", "path"},
        {"map", "Name of the map to export", "name"},
    },
};

const QHash<QString, const Command*> commands = {
    /*{
        "convert"
    }*/
    {"exportimage", &exportimage},
};



CommandLine::CommandLine(QWidget *parent) :
    QObject(parent)
{
    parser.setApplicationDescription("A map editor for the Pokémon generation 3 decompilation projects pokeruby, pokeemerald, and pokefirered.");
}

CommandLine::~CommandLine() 
{
    delete project;
}

bool CommandLine::isCommand(QString s) {
    return commands.contains(s);
}

/*
   
*/
bool CommandLine::parse() {
    // Build description of each command for the --help text.
    parser.addPositionalArgument("command", QString("The command to execute. (%1)").arg(commands.keys().join(" | ")));
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();

    // Don't process the arguments yet or any command-specific options will throw an error
    args = QCoreApplication::arguments();
    bool success = parser.parse(args);

    for (auto arg : parser.positionalArguments()) {
        if (isCommand(arg)) {
            // We only consider one command at a time
            commandName = arg;

            // exec() hasn't been called in main yet. queue application to quit when run() finishes
            QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
            return true;
        }
    }

    // No commands found, process built-in options and throw errors.
    if (parser.isSet(helpOption)) {
        parser.showHelp(0);
        return true;
    }
    if (parser.isSet(versionOption)){
        parser.showVersion();
        return true;
    }
    if (!success) {
        fputs(qPrintable(parser.errorText() + "\n\n"), stderr);
        parser.showHelp(1);
        return true;
    }

    return false;
}

void CommandLine::run() {
    if (commandName.isEmpty())
        return;

    const Command * cmd = commands.value(commandName);
    parser.clearPositionalArguments();
    parser.addOptions(cmd->options);
    parser.process(args);


}
/*
void CommandLine::exportMapImage() {

}
*/