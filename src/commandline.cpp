#include "commandline.h"
#include "config.h"
#include "log.h"

const QString exportImage = "exportimage";
const QString convert = "convert";

const QSet<QString> commandNames = {
    exportImage,
    convert,
};

CommandLine::~CommandLine()  {
    delete project;
}

bool CommandLine::isCommand(QString s) {
    return commandNames.contains(s);
}

bool CommandLine::parse() {
    this->parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);
    this->parser.addPositionalArgument("command", "The command to execute", QString("(%1) [<args>]").arg(commandNames.values().join("|")));
    const QCommandLineOption helpOption = this->parser.addHelpOption();
    const QCommandLineOption versionOption = this->parser.addVersionOption();

    // Don't process the arguments yet or any command-specific options will throw an error
    this->args = QCoreApplication::arguments();
    bool success = parser.parse(this->args);

    for (auto arg : parser.positionalArguments()) {
        if (this->isCommand(arg)) {
            // We only consider one command at a time
            this->commandName = arg;

            // exec() hasn't been called in main yet. queue application to quit when run() finishes
            QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
            return true;
        }
    }

    // No commands found, process built-in options and throw errors.
    if (this->parser.isSet(helpOption)) {
        this->parser.showHelp();
        return true;
    }
    if (this->parser.isSet(versionOption)){
        this->parser.showVersion();
        return true;
    }
    if (!success) {
        this->parser.showError();
        return true;
    }

    return false;
}

/*

*/
void CommandLine::run() {
    porymapConfig.load();

    this->parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    this->parser.clearPositionalArguments();

    // TODO: Handle diverting to command functions more elegantly
    /*
    Command * cmd = ;
    this->parser.addCommandArgument(cmd->name, cmd->description, cmd->options);
    this->parser.process(this->args);
    cmd
    */
    if (this->commandName == exportImage)
        this->runExportImage();
    else {
        this->parser.showError(QString("ERROR: Unrecognized command '%1'").arg(this->commandName));
        return;
    }
}


// Generic options used by multiple commands
const QCommandLineOption option_output = {{"output", "o"}, "Output file path", "path"};
const QCommandLineOption option_project = {{"project", "p"}, "Project file path", "path"};

bool CommandLine::loadProject() {
    if (this->project)
        return true;

    QString dir = this->parser.value(option_project);
    if (dir.isEmpty())
        dir = porymapConfig.getRecentProject();

    QFileInfo fileInfo(dir);
    if (!fileInfo.exists() || !fileInfo.isDir()) {
        logError(QString("Project folder '%1' does not exist.").arg(dir));
        return false;
    }

    this->project = new Project(this);
    this->project->set_root(dir);
    if (!this->project->readData() || !this->project->readMapGroups()) {
        // Assume an error was already reported when the project failed to load
        delete this->project;
        this->project = nullptr;
        return false;
    }

    return true;
}

void CommandLine::runExportImage() {
    const QCommandLineOption option_map = {"map", "Name of the map to export", "name"};
    const QCommandLineOption option_stitch = {"stitch", "Include all connected maps in the image"};
    const QCommandLineOption option_all = {"all", "Export an image for every map"};
    QList<QCommandLineOption> options = {
        option_map,
        option_stitch,
        option_all,
        option_output,
        option_project,
    };
    this->parser.addCommandArgument(exportImage, "Export a map image", options);
    this->parser.process(this->args);

    if (this->parser.checkExclusiveOptions(option_all, option_map)
     || this->parser.checkExclusiveOptions(option_all, option_stitch))
        return;

    if (!this->loadProject())
        return;

    bool allMaps = this->parser.isSet(option_all);

    const QStringList maps = allMaps ? this->project->mapNames : this->parser.values(option_map);
    const QStringList paths = this->parser.values(option_output);

    if (maps.isEmpty()) {
        this->parser.showError("ERROR: No map specified");
        return;
    }

    if (allMaps) {
        if (paths.length() < 1) {
            this->parser.showError("ERROR: One '--output' target directory must be specified.");
            return;
        }
        // TODO: Verify output directory, create it(?)
    } else if (paths.length() < maps.length()) {
        // If --all wasn't used, an output path must be specified for every map
        this->parser.showError("ERROR: One '--output' file path must be specified for every map image.");
        return;
    }

    logInfo(maps.join(","));
}

