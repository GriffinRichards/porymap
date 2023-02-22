#include "commandline.h"
#include "config.h"
#include "log.h"
#include "mapimageexporter.h"

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


// Generic options used by multiple commands
const QCommandLineOption option_output = {{"output", "o"}, "Output file path", "path"};
const QCommandLineOption option_project = {{"project", "p"}, "Project file path", "path"};

/*

*/
void CommandLine::run() {
    this->parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    this->parser.clearPositionalArguments();
    this->parser.addSilentOption();

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
        this->parser.showError(QString("Unrecognized command '%1'").arg(this->commandName));
        return;
    }
}

bool CommandLine::loadProject() {
    if (this->project)
        return true;

    porymapConfig.load();
    QString dir = this->parser.value(option_project);
    if (dir.isEmpty())
        dir = porymapConfig.getRecentProject();

    QFileInfo fileInfo(dir);
    if (!fileInfo.exists() || !fileInfo.isDir()) {
        logError(QString("Project folder '%1' does not exist.").arg(dir));
        return false;
    }

    userConfig.setProjectDir(dir);
    userConfig.load();
    projectConfig.setProjectDir(dir);
    projectConfig.load();

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

    const QStringList mapNames = allMaps ? this->project->mapNames : this->parser.values(option_map);
    const QStringList paths = this->parser.values(option_output);

    if (mapNames.isEmpty()) {
        this->parser.showError("No map specified.");
        return;
    }

    if (allMaps) {
        if (paths.length() < 1) {
            this->parser.showError("An '--output' target directory must be specified.");
            return;
        }
        // TODO: Verify output directory, create it(?)
    } else if (paths.length() < mapNames.length()) {
        this->parser.showError("An '--output' file path must be specified for every map image.");
        return;
    }

    // TODO: Maybe just consume arguments in order? Should options apply universally or to each map?
    int numFailed = 0;
    for (int i = 0; i < mapNames.length(); i++) {
        const QString mapName = mapNames.at(i);

        // TODO: Verify that it's a valid map name
        if (mapName == DYNAMIC_MAP_NAME)
            continue;

        Map * map = this->project->getMap(mapName);
        if (!map) {
            // Assume an error was reported by the project
            numFailed++;
            continue;
        }

        this->parser.showMessage(QString("Exporting image for '%1'...").arg(mapName));

        // Either read the specified path, or create one if --all was used
        QString outputPath = !allMaps ? paths.at(i) : QString("%1/%2.png").arg(paths.at(0)).arg(mapName);
        // TODO: Verify the output path is valid (dir exists, file doesn't already exist (--overwrite/--force?), file extension? etc.)

        MapImageExportSettings settings;
        QPixmap pixmap = MapImageExporter::getFormattedMapPixmap(map, settings);
        if (!pixmap.save(outputPath)) {
            this->parser.showMessage(QString("Unable to write image file '%1'").arg(outputPath));
            numFailed++;
        }
    }

    QString endMessage = QString("Image export complete!");
    if (numFailed) endMessage.append(QString(" Failed to export %1 image(s).").arg(numFailed));
    this->parser.showMessage(endMessage);
}

