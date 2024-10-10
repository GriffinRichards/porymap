#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "project.h"
#include "log.h"
#include "editor.h"
#include "prefabcreationdialog.h"
#include "eventframes.h"
#include "bordermetatilespixmapitem.h"
#include "currentselectedmetatilespixmapitem.h"
#include "customattributestable.h"
#include "scripting.h"
#include "adjustingstackedwidget.h"
#include "draggablepixmapitem.h"
#include "editcommands.h"
#include "flowlayout.h"
#include "shortcut.h"
#include "mapparser.h"
#include "prefab.h"
#include "montabwidget.h"
#include "imageexport.h"
#include "newmapconnectiondialog.h"

#include <QFileDialog>
#include <QClipboard>
#include <QDirIterator>
#include <QStandardItemModel>
#include <QSpinBox>
#include <QTextEdit>
#include <QSpacerItem>
#include <QFont>
#include <QScrollBar>
#include <QPushButton>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QScroller>
#include <math.h>
#include <QSysInfo>
#include <QDesktopServices>
#include <QTransform>
#include <QSignalBlocker>
#include <QSet>
#include <QLoggingCategory>

// We only publish release binaries for Windows and macOS.
// This is relevant for the update promoter, which alerts users of a new release.
// TODO: Currently the update promoter is disabled on our Windows releases because
//       the pre-compiled Qt build doesn't link OpenSSL. Re-enable below once this is fixed.
#if /*defined(Q_OS_WIN) || */defined(Q_OS_MACOS)
#define RELEASE_PLATFORM
#endif

using OrderedJson = poryjson::Json;
using OrderedJsonDoc = poryjson::JsonDoc;



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QCoreApplication::setOrganizationName("pret");
    QCoreApplication::setApplicationName("porymap");
    QCoreApplication::setApplicationVersion(PORYMAP_VERSION);
    QApplication::setApplicationDisplayName("porymap");
    QApplication::setWindowIcon(QIcon(":/icons/porymap-icon-2.ico"));
    ui->setupUi(this);

    cleanupLargeLog();
    logInfo(QString("Launching Porymap v%1").arg(QCoreApplication::applicationVersion()));

    this->initWindow();
    if (porymapConfig.reopenOnLaunch && !porymapConfig.projectManuallyClosed && this->openProject(porymapConfig.getRecentProject(), true))
        on_toolButton_Paint_clicked();

    // there is a bug affecting macOS users, where the trackpad deilveres a bad touch-release gesture
    // the warning is a bit annoying, so it is disabled here
    QLoggingCategory::setFilterRules(QStringLiteral("qt.pointer.dispatch=false"));

    if (porymapConfig.checkForUpdates)
        this->checkForUpdates(false);
}

MainWindow::~MainWindow()
{
    // Some config settings are updated as subwindows are destroyed (e.g. their geometry),
    // so we need to ensure that the configs are saved after this happens.
    saveGlobalConfigs();

    delete label_MapRulerStatus;
    delete editor;
    delete mapListProxyModel;
    delete mapGroupItemsList;
    delete mapListModel;
    delete ui;
}

void MainWindow::saveGlobalConfigs() {
    porymapConfig.setMainGeometry(
        this->saveGeometry(),
        this->saveState(),
        this->ui->splitter_map->saveState(),
        this->ui->splitter_main->saveState(),
        this->ui->splitter_Metatiles->saveState()
    );
    porymapConfig.save();
    shortcutsConfig.save();
}

void MainWindow::setWindowDisabled(bool disabled) {
    for (auto action : findChildren<QAction *>())
        action->setDisabled(disabled);
    for (auto child : findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        child->setDisabled(disabled);
    for (auto menu : ui->menuBar->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly))
        menu->setDisabled(disabled);
    ui->menuBar->setDisabled(false);
    ui->menuFile->setDisabled(false);
    ui->action_Open_Project->setDisabled(false);
    ui->menuOpen_Recent_Project->setDisabled(false);
    refreshRecentProjectsMenu();
    ui->action_Exit->setDisabled(false);
    ui->menuHelp->setDisabled(false);
    ui->actionAbout_Porymap->setDisabled(false);
    ui->actionOpen_Log_File->setDisabled(false);
    ui->actionOpen_Config_Folder->setDisabled(false);
    ui->actionCheck_for_Updates->setDisabled(false);
    if (!disabled)
        togglePreferenceSpecificUi();
}

void MainWindow::initWindow() {
    porymapConfig.load();
    this->initCustomUI();
    this->initExtraSignals();
    this->initEditor();
    this->initMiscHeapObjects();
    this->initMapSortOrder();
    this->initShortcuts();
    this->restoreWindowState();

#ifndef RELEASE_PLATFORM
    ui->actionCheck_for_Updates->setVisible(false);
#endif

#ifdef DISABLE_CHARTS_MODULE
    ui->pushButton_SummaryChart->setVisible(false);
#endif

    setWindowDisabled(true);
}

void MainWindow::initShortcuts() {
    initExtraShortcuts();

    shortcutsConfig.load();
    shortcutsConfig.setDefaultShortcuts(shortcutableObjects());
    applyUserShortcuts();
}

void MainWindow::initExtraShortcuts() {
    ui->actionZoom_In->setShortcuts({ui->actionZoom_In->shortcut(), QKeySequence("Ctrl+=")});

    auto *shortcutReset_Zoom = new Shortcut(QKeySequence("Ctrl+0"), this, SLOT(resetMapViewScale()));
    shortcutReset_Zoom->setObjectName("shortcutZoom_Reset");
    shortcutReset_Zoom->setWhatsThis("Zoom Reset");

    auto *shortcutDuplicate_Events = new Shortcut(QKeySequence("Ctrl+D"), this, SLOT(duplicate()));
    shortcutDuplicate_Events->setObjectName("shortcutDuplicate_Events");
    shortcutDuplicate_Events->setWhatsThis("Duplicate Selected Event(s)");

    auto *shortcutDelete_Object = new Shortcut(
            {QKeySequence("Del"), QKeySequence("Backspace")}, this, SLOT(onDeleteKeyPressed()));
    shortcutDelete_Object->setObjectName("shortcutDelete_Object");
    shortcutDelete_Object->setWhatsThis("Delete Selected Item(s)");

    auto *shortcutToggle_Border = new Shortcut(QKeySequence(), ui->checkBox_ToggleBorder, SLOT(toggle()));
    shortcutToggle_Border->setObjectName("shortcutToggle_Border");
    shortcutToggle_Border->setWhatsThis("Toggle Border");

    auto *shortcutToggle_Smart_Paths = new Shortcut(QKeySequence(), ui->checkBox_smartPaths, SLOT(toggle()));
    shortcutToggle_Smart_Paths->setObjectName("shortcutToggle_Smart_Paths");
    shortcutToggle_Smart_Paths->setWhatsThis("Toggle Smart Paths");

    auto *shortcutExpand_All = new Shortcut(QKeySequence(), this, SLOT(on_toolButton_ExpandAll_clicked()));
    shortcutExpand_All->setObjectName("shortcutExpand_All");
    shortcutExpand_All->setWhatsThis("Map List: Expand all folders");

    auto *shortcutCollapse_All = new Shortcut(QKeySequence(), this, SLOT(on_toolButton_CollapseAll_clicked()));
    shortcutCollapse_All->setObjectName("shortcutCollapse_All");
    shortcutCollapse_All->setWhatsThis("Map List: Collapse all folders");

    auto *shortcut_Open_Scripts = new Shortcut(QKeySequence(), ui->toolButton_Open_Scripts, SLOT(click()));
    shortcut_Open_Scripts->setObjectName("shortcut_Open_Scripts");
    shortcut_Open_Scripts->setWhatsThis("Open Map Scripts");

    copyAction = new QAction("Copy", this);
    copyAction->setShortcut(QKeySequence("Ctrl+C"));
    connect(copyAction, &QAction::triggered, this, &MainWindow::copy);
    ui->menuEdit->addSeparator();
    ui->menuEdit->addAction(copyAction);

    pasteAction = new QAction("Paste", this);
    pasteAction->setShortcut(QKeySequence("Ctrl+V"));
    connect(pasteAction, &QAction::triggered, this, &MainWindow::paste);
    ui->menuEdit->addAction(pasteAction);
}

QObjectList MainWindow::shortcutableObjects() const {
    QObjectList shortcutable_objects;

    for (auto *action : findChildren<QAction *>())
        if (!action->objectName().isEmpty())
            shortcutable_objects.append(qobject_cast<QObject *>(action));
    for (auto *shortcut : findChildren<Shortcut *>())
        if (!shortcut->objectName().isEmpty())
            shortcutable_objects.append(qobject_cast<QObject *>(shortcut));

    return shortcutable_objects;
}

void MainWindow::applyUserShortcuts() {
    for (auto *action : findChildren<QAction *>())
        if (!action->objectName().isEmpty())
            action->setShortcuts(shortcutsConfig.userShortcuts(action));
    for (auto *shortcut : findChildren<Shortcut *>())
        if (!shortcut->objectName().isEmpty())
            shortcut->setKeys(shortcutsConfig.userShortcuts(shortcut));
}

static const QMap<int, QString> mainTabNames = {
    {MainTab::Map, "Map"},
    {MainTab::Events, "Events"},
    {MainTab::Header, "Header"},
    {MainTab::Connections, "Connections"},
    {MainTab::WildPokemon, "Wild Pokemon"},
};

void MainWindow::initCustomUI() {
    // Set up the tab bar
    while (ui->mainTabBar->count()) ui->mainTabBar->removeTab(0);

    for (int i = 0; i < mainTabNames.count(); i++)
        ui->mainTabBar->addTab(mainTabNames.value(i));

    ui->mainTabBar->setTabIcon(MainTab::Map, QIcon(QStringLiteral(":/icons/map.ico")));
    ui->mainTabBar->setTabIcon(MainTab::WildPokemon, QIcon(QStringLiteral(":/icons/tall_grass.ico")));

    // Create map header data widget
    this->mapHeader = new MapHeaderForm();
    ui->layout_HeaderData->addWidget(this->mapHeader);
}

void MainWindow::initExtraSignals() {
    // Right-clicking on items in the map list tree view brings up a context menu.
    ui->mapList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->mapList, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onOpenMapListContextMenu);

    // Change pages on wild encounter groups
    connect(ui->comboBox_EncounterGroupLabel, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index){
        ui->stackedWidget_WildMons->setCurrentIndex(index);
    });

    // Convert the layout of the map tools' frame into an adjustable FlowLayout
    FlowLayout *flowLayout = new FlowLayout;
    flowLayout->setContentsMargins(ui->frame_mapTools->layout()->contentsMargins());
    flowLayout->setSpacing(ui->frame_mapTools->layout()->spacing());
    for (auto *child : ui->frame_mapTools->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
        flowLayout->addWidget(child);
        child->setFixedHeight(
            ui->frame_mapTools->height() - flowLayout->contentsMargins().top() - flowLayout->contentsMargins().bottom()
        );
    }
    delete ui->frame_mapTools->layout();
    ui->frame_mapTools->setLayout(flowLayout);

    // Floating QLabel tool-window that displays over the map when the ruler is active
    label_MapRulerStatus = new QLabel(ui->graphicsView_Map);
    label_MapRulerStatus->setObjectName("label_MapRulerStatus");
    label_MapRulerStatus->setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    label_MapRulerStatus->setFrameShape(QFrame::Box);
    label_MapRulerStatus->setMargin(3);
    label_MapRulerStatus->setPalette(palette());
    label_MapRulerStatus->setAlignment(Qt::AlignCenter);
    label_MapRulerStatus->setTextFormat(Qt::PlainText);
    label_MapRulerStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

void MainWindow::on_actionCheck_for_Updates_triggered() {
    checkForUpdates(true);
}

#ifdef RELEASE_PLATFORM
void MainWindow::checkForUpdates(bool requestedByUser) {
    if (!this->networkAccessManager)
        this->networkAccessManager = new NetworkAccessManager(this);

    if (!this->updatePromoter) {
        this->updatePromoter = new UpdatePromoter(this, this->networkAccessManager);
        connect(this->updatePromoter, &UpdatePromoter::changedPreferences, [this] {
            if (this->preferenceEditor)
                this->preferenceEditor->updateFields();
        });
    }


    if (requestedByUser) {
        openSubWindow(this->updatePromoter);
    } else {
        // This is an automatic update check. Only run if we haven't done one in the last 5 minutes
        QDateTime lastCheck = porymapConfig.lastUpdateCheckTime;
        if (lastCheck.addSecs(5*60) >= QDateTime::currentDateTime())
            return;
    }
    this->updatePromoter->checkForUpdates();
    porymapConfig.lastUpdateCheckTime = QDateTime::currentDateTime();
}
#else
void MainWindow::checkForUpdates(bool) {}
#endif

void MainWindow::initEditor() {
    this->editor = new Editor(ui);
    connect(this->editor, &Editor::mapEventsCleared, this, &MainWindow::clearEventsPanel);
    connect(this->editor, &Editor::mapEventsDisplayed, this, &MainWindow::refreshEventsPanel);
    connect(this->editor, &Editor::selectedEventsChanged, this, &MainWindow::refreshSelectedEventsTab);
    connect(this->editor, &Editor::openConnectedMap, this, &MainWindow::onOpenConnectedMap);
    connect(this->editor, &Editor::warpEventDoubleClicked, this, &MainWindow::openWarpMap);
    connect(this->editor, &Editor::currentMetatilesSelectionChanged, this, &MainWindow::currentMetatilesSelectionChanged);
    connect(this->editor, &Editor::wildMonTableEdited, [this] { this->markMapEdited(); });
    connect(this->editor, &Editor::mapRulerStatusChanged, this, &MainWindow::onMapRulerStatusChanged);
    connect(this->editor, &Editor::tilesetUpdated, this, &Scripting::cb_TilesetUpdated);

    this->loadUserSettings();

    undoAction = editor->editGroup.createUndoAction(this, tr("&Undo"));
    undoAction->setObjectName("action_Undo");
    undoAction->setShortcut(QKeySequence("Ctrl+Z"));

    redoAction = editor->editGroup.createRedoAction(this, tr("&Redo"));
    redoAction->setObjectName("action_Redo");
    redoAction->setShortcuts({QKeySequence("Ctrl+Y"), QKeySequence("Ctrl+Shift+Z")});

    ui->menuEdit->addAction(undoAction);
    ui->menuEdit->addAction(redoAction);

    QUndoView *undoView = new QUndoView(&editor->editGroup);
    undoView->setWindowTitle(tr("Edit History"));
    undoView->setAttribute(Qt::WA_QuitOnClose, false);

    // Show the EditHistory dialog with Ctrl+E
    QAction *showHistory = new QAction("Show Edit History...", this);
    showHistory->setObjectName("action_ShowEditHistory");
    showHistory->setShortcut(QKeySequence("Ctrl+E"));
    connect(showHistory, &QAction::triggered, [this, undoView](){ openSubWindow(undoView); });

    ui->menuEdit->addAction(showHistory);

    // Toggle an asterisk in the window title when the undo state is changed
    connect(&editor->editGroup, &QUndoGroup::cleanChanged, this, &MainWindow::showWindowTitle);
}

void MainWindow::initMiscHeapObjects() {
    mapIcon = QIcon(QStringLiteral(":/icons/map.ico"));

    mapListModel = new QStandardItemModel;
    mapGroupItemsList = new QList<QStandardItem*>;
    mapListProxyModel = new FilterChildrenProxyModel;

    mapListProxyModel->setSourceModel(mapListModel);
    ui->mapList->setModel(mapListProxyModel);
}

void MainWindow::initMapSortOrder() {
    QMenu *mapSortOrderMenu = new QMenu(this);
    QActionGroup *mapSortOrderActionGroup = new QActionGroup(ui->toolButton_MapSortOrder);

    mapSortOrderMenu->addAction(ui->actionSort_by_Group);
    mapSortOrderMenu->addAction(ui->actionSort_by_Area);
    mapSortOrderMenu->addAction(ui->actionSort_by_Layout);
    ui->toolButton_MapSortOrder->setMenu(mapSortOrderMenu);

    mapSortOrderActionGroup->addAction(ui->actionSort_by_Group);
    mapSortOrderActionGroup->addAction(ui->actionSort_by_Area);
    mapSortOrderActionGroup->addAction(ui->actionSort_by_Layout);

    connect(mapSortOrderActionGroup, &QActionGroup::triggered, this, &MainWindow::mapSortOrder_changed);

    QAction* sortOrder = ui->toolButton_MapSortOrder->menu()->actions()[porymapConfig.mapSortOrder];
    ui->toolButton_MapSortOrder->setIcon(sortOrder->icon());
    sortOrder->setChecked(true);
}

void MainWindow::showWindowTitle() {
    if (editor->map) {
        setWindowTitle(QString("%1%2 - %3")
            .arg(editor->map->hasUnsavedChanges() ? "* " : "")
            .arg(editor->map->name)
            .arg(editor->project->getProjectTitle())
        );
    }
}

void MainWindow::markMapEdited() {
    if (editor) markMapEdited(editor->map);
}

void MainWindow::markMapEdited(Map* map) {
    if (!map)
        return;
    map->hasUnsavedDataChanges = true;

    updateMapListIcon(map->name);
    if (editor && editor->map == map)
        showWindowTitle();
}

void MainWindow::mapSortOrder_changed(QAction *action)
{
    QList<QAction*> items = ui->toolButton_MapSortOrder->menu()->actions();
    int i = 0;
    for (; i < items.count(); i++)
    {
        if (items[i] == action)
        {
            break;
        }
    }

    if (i != porymapConfig.mapSortOrder)
    {
        ui->toolButton_MapSortOrder->setIcon(action->icon());
        porymapConfig.mapSortOrder = static_cast<MapSortOrder>(i);
        if (isProjectOpen())
        {
            sortMapList();
            applyMapListFilter(ui->lineEdit_filterBox->text());
        }
    }
}

void MainWindow::on_lineEdit_filterBox_textChanged(const QString &arg1)
{
    this->applyMapListFilter(arg1);
}

void MainWindow::applyMapListFilter(QString filterText)
{
    mapListProxyModel->setFilterRegularExpression(QRegularExpression(filterText, QRegularExpression::CaseInsensitiveOption));
    if (filterText.isEmpty()) {
        ui->mapList->collapseAll();
    } else {
        ui->mapList->expandToDepth(0);
    }
    ui->mapList->setExpanded(mapListProxyModel->mapFromSource(mapListIndexes.value(editor->map->name)), true);
    ui->mapList->scrollTo(mapListProxyModel->mapFromSource(mapListIndexes.value(editor->map->name)), QAbstractItemView::PositionAtCenter);
}

void MainWindow::loadUserSettings() {
    // Better Cursors
    ui->actionBetter_Cursors->setChecked(porymapConfig.prettyCursors);
    this->editor->settings->betterCursors = porymapConfig.prettyCursors;

    // Player view rectangle
    ui->actionPlayer_View_Rectangle->setChecked(porymapConfig.showPlayerView);
    this->editor->settings->playerViewRectEnabled = porymapConfig.showPlayerView;

    // Cursor tile outline
    ui->actionCursor_Tile_Outline->setChecked(porymapConfig.showCursorTile);
    this->editor->settings->cursorTileRectEnabled = porymapConfig.showCursorTile;

    // Border
    ui->checkBox_ToggleBorder->setChecked(porymapConfig.showBorder);

    // Grid
    const QSignalBlocker b_Grid(ui->checkBox_ToggleGrid);
    ui->actionShow_Grid->setChecked(porymapConfig.showGrid);
    ui->checkBox_ToggleGrid->setChecked(porymapConfig.showGrid);

    // Mirror connections
    ui->checkBox_MirrorConnections->setChecked(porymapConfig.mirrorConnectingMaps);

    // Collision opacity/transparency
    const QSignalBlocker b_CollisionTransparency(ui->horizontalSlider_CollisionTransparency);
    this->editor->collisionOpacity = static_cast<qreal>(porymapConfig.collisionOpacity) / 100;
    ui->horizontalSlider_CollisionTransparency->setValue(porymapConfig.collisionOpacity);

    // Dive map opacity/transparency
    const QSignalBlocker b_DiveEmergeOpacity(ui->slider_DiveEmergeMapOpacity);
    const QSignalBlocker b_DiveMapOpacity(ui->slider_DiveMapOpacity);
    const QSignalBlocker b_EmergeMapOpacity(ui->slider_EmergeMapOpacity);
    ui->slider_DiveEmergeMapOpacity->setValue(porymapConfig.diveEmergeMapOpacity);
    ui->slider_DiveMapOpacity->setValue(porymapConfig.diveMapOpacity);
    ui->slider_EmergeMapOpacity->setValue(porymapConfig.emergeMapOpacity);

    // Zoom
    const QSignalBlocker b_MetatileZoom(ui->horizontalSlider_MetatileZoom);
    const QSignalBlocker b_CollisionZoom(ui->horizontalSlider_CollisionZoom);
    ui->horizontalSlider_MetatileZoom->setValue(porymapConfig.metatilesZoom);
    ui->horizontalSlider_CollisionZoom->setValue(porymapConfig.collisionZoom);

    setTheme(porymapConfig.theme);
    setDivingMapsVisible(porymapConfig.showDiveEmergeMaps);
}

void MainWindow::restoreWindowState() {
    logInfo("Restoring main window geometry from previous session.");
    QMap<QString, QByteArray> geometry = porymapConfig.getMainGeometry();
    this->restoreGeometry(geometry.value("main_window_geometry"));
    this->restoreState(geometry.value("main_window_state"));
    this->ui->splitter_map->restoreState(geometry.value("map_splitter_state"));
    this->ui->splitter_main->restoreState(geometry.value("main_splitter_state"));
    this->ui->splitter_Metatiles->restoreState(geometry.value("metatiles_splitter_state"));
}

void MainWindow::setTheme(QString theme) {
    if (theme == "default") {
        setStyleSheet("");
    } else {
        QFile File(QString(":/themes/%1.qss").arg(theme));
        File.open(QFile::ReadOnly);
        QString stylesheet = QLatin1String(File.readAll());
        setStyleSheet(stylesheet);
    }
}

bool MainWindow::openProject(QString dir, bool initial) {
    if (dir.isNull() || dir.length() <= 0) {
        // If this happened on startup it's because the user has no recent projects, which is fine.
        // This shouldn't happen otherwise, but if it does then display an error.
        if (!initial) {
            logError("Failed to open project: Directory name cannot be empty");
            showProjectOpenFailure();
        }
        return false;
    }

    const QString projectString = QString("%1project '%2'").arg(initial ? "recent " : "").arg(QDir::toNativeSeparators(dir));

    if (!QDir(dir).exists()) {
        const QString errorMsg = QString("Failed to open %1: No such directory").arg(projectString);
        this->statusBar()->showMessage(errorMsg);
        if (initial) {
            // Graceful startup if recent project directory is missing
            logWarn(errorMsg);
        } else {
            logError(errorMsg);
            showProjectOpenFailure();
        }
        return false;
    }

    // The above checks can fail and the user will be allowed to continue with their currently-opened project (if there is one).
    // We close the current project below, after which either the new project will open successfully or the window will be disabled.
    if (!closeProject()) {
        logInfo("Aborted project open.");
        return false;
    }

    const QString openMessage = QString("Opening %1").arg(projectString);
    this->statusBar()->showMessage(openMessage);
    logInfo(openMessage);

    userConfig.projectDir = dir;
    userConfig.load();
    projectConfig.projectDir = dir;
    projectConfig.load();

    this->newMapDefaultsSet = false;

    Scripting::init(this);

    // Create the project
    auto project = new Project(this);
    project->set_root(dir);
    QObject::connect(project, &Project::reloadProject, this, &MainWindow::on_action_Reload_Project_triggered);
    QObject::connect(project, &Project::mapCacheCleared, this, &MainWindow::onMapCacheCleared);
    QObject::connect(project, &Project::mapLoaded, this, &MainWindow::onMapLoaded);
    QObject::connect(project, &Project::uncheckMonitorFilesAction, [this]() {
        porymapConfig.monitorFiles = false;
        if (this->preferenceEditor)
            this->preferenceEditor->updateFields();
    });
    this->editor->setProject(project);

    // Make sure project looks reasonable before attempting to load it
    if (!checkProjectSanity()) {
        delete this->editor->project;
        return false;
    }

    // Load the project
    if (!(loadProjectData() && setProjectUI() && setInitialMap())) {
        this->statusBar()->showMessage(QString("Failed to open %1").arg(projectString));
        showProjectOpenFailure();
        delete this->editor->project;
        // TODO: Allow changing project settings at this point
        return false;
    }

    // Only create the config files once the project has opened successfully in case the user selected an invalid directory
    this->editor->project->saveConfig();
    
    showWindowTitle();
    this->statusBar()->showMessage(QString("Opened %1").arg(projectString));

    porymapConfig.projectManuallyClosed = false;
    porymapConfig.addRecentProject(dir);
    refreshRecentProjectsMenu();

    prefab.initPrefabUI(
                editor->metatile_selector_item,
                ui->scrollAreaWidgetContents_Prefabs,
                ui->label_prefabHelp,
                editor->map);
    Scripting::cb_ProjectOpened(dir);
    setWindowDisabled(false);
    return true;
}

bool MainWindow::loadProjectData() {
    bool success = editor->project->load();
    Scripting::populateGlobalObject(this);
    return success;
}

bool MainWindow::checkProjectSanity() {
    if (editor->project->sanityCheck())
        return true;

    logWarn(QString("The directory '%1' failed the project sanity check.").arg(editor->project->root));

    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText(QString("The selected directory appears to be invalid."));
    msgBox.setInformativeText(QString("The directory '%1' is missing key files.\n\n"
                                      "Make sure you selected the correct project directory "
                                      "(the one used to make your .gba file, e.g. 'pokeemerald').").arg(editor->project->root));
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    auto tryAnyway = msgBox.addButton("Try Anyway", QMessageBox::ActionRole);
    msgBox.exec();
    if (msgBox.clickedButton() == tryAnyway) {
        // The user has chosen to try to load this project anyway.
        // This will almost certainly fail, but they'll get a more specific error message.
        return true;
    }
    return false;
}

void MainWindow::showProjectOpenFailure() {
    QString errorMsg = QString("There was an error opening the project. Please see %1 for full error details.").arg(getLogPath());
    QMessageBox error(QMessageBox::Critical, "porymap", errorMsg, QMessageBox::Ok, this);
    error.setDetailedText(getMostRecentError());
    error.exec();
}

bool MainWindow::isProjectOpen() {
    return editor && editor->project;
}

bool MainWindow::setInitialMap() {
    QStringList names;
    if (editor && editor->project)
        names = editor->project->mapNames;

    // Try to set most recently-opened map, if it's still in the list.
    QString recentMap = userConfig.recentMap;
    if (!recentMap.isEmpty() && names.contains(recentMap) && setMap(recentMap, true))
        return true;

    // Failing that, try loading maps in the map list sequentially.
    for (auto name : names) {
        if (name != recentMap && setMap(name, true))
            return true;
    }

    logError("Failed to load any maps.");
    return false;
}

void MainWindow::refreshRecentProjectsMenu() {
    ui->menuOpen_Recent_Project->clear();
    QStringList recentProjects = porymapConfig.getRecentProjects();

    if (isProjectOpen()) {
        // Don't show the currently open project in this menu
        recentProjects.removeOne(this->editor->project->root);
    }

    // Add project paths to menu. Skip any paths to folders that don't exist
    for (int i = 0; i < recentProjects.length(); i++) {
        const QString path = recentProjects.at(i);
        if (QDir(path).exists()) {
            ui->menuOpen_Recent_Project->addAction(path, [this, path](){
                this->openProject(path);
            });
        }
        // Arbitrary limit of 10 items.
        if (ui->menuOpen_Recent_Project->actions().length() >= 10)
            break;
    }

    // Add action to clear list of paths
    if (!ui->menuOpen_Recent_Project->actions().isEmpty())
         ui->menuOpen_Recent_Project->addSeparator();
    QAction *clearAction = ui->menuOpen_Recent_Project->addAction("Clear Items", [this](){
        QStringList paths = QStringList();
        if (isProjectOpen())
            paths.append(this->editor->project->root);
        porymapConfig.setRecentProjects(paths);
        this->refreshRecentProjectsMenu();
    });
    clearAction->setEnabled(!recentProjects.isEmpty());
}

void MainWindow::openSubWindow(QWidget * window) {
    if (!window) return;

    if (!window->isVisible()) {
        window->show();
    } else if (window->isMinimized()) {
        window->showNormal();
    } else {
        window->raise();
        window->activateWindow();
    }
}

QString MainWindow::getExistingDirectory(QString dir) {
    return QFileDialog::getExistingDirectory(this, "Open Directory", dir, QFileDialog::ShowDirsOnly);
}

void MainWindow::on_action_Open_Project_triggered()
{
    QString dir = getExistingDirectory(!userConfig.recentMap.isEmpty() ? userConfig.recentMap : ".");
    if (!dir.isEmpty())
        openProject(dir);
}

void MainWindow::on_action_Reload_Project_triggered() {
    // TODO: when undo history is complete show only if has unsaved changes
    QMessageBox warning(this);
    warning.setText("WARNING");
    warning.setInformativeText("Reloading this project will discard any unsaved changes.");
    warning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    warning.setDefaultButton(QMessageBox::Cancel);
    warning.setIcon(QMessageBox::Warning);

    if (warning.exec() == QMessageBox::Ok)
        openProject(editor->project->root);
}

void MainWindow::on_action_Close_Project_triggered() {
    closeProject();
    porymapConfig.projectManuallyClosed = true;
}

// setMap, but with a visible error message in case of failure.
// Use when the user is specifically requesting a map to open.
bool MainWindow::userSetMap(QString map_name, bool scrollTreeView) {
    if (editor->map && editor->map->name == map_name)
        return true; // Already set

    if (map_name == editor->project->getDynamicMapName()) {
        QMessageBox msgBox(this);
        QString errorMsg = QString("The map '%1' can't be opened, it's a placeholder to indicate the specified map will be set programmatically.").arg(map_name);
        msgBox.critical(nullptr, "Error Opening Map", errorMsg);
        return false;
    }

    if (!setMap(map_name, scrollTreeView)) {
        QMessageBox msgBox(this);
        QString errorMsg = QString("There was an error opening map %1. Please see %2 for full error details.\n\n%3")
                .arg(map_name)
                .arg(getLogPath())
                .arg(getMostRecentError());
        msgBox.critical(nullptr, "Error Opening Map", errorMsg);
        return false;
    }
    return true;
}

bool MainWindow::setMap(QString map_name, bool scrollTreeView) {
    if (!editor || !editor->project || map_name.isEmpty() || map_name == editor->project->getDynamicMapName()) {
        logWarn(QString("Ignored setting map to '%1'").arg(map_name));
        return false;
    }

    logInfo(QString("Setting map to '%1'").arg(map_name));
    if (!editor->setMap(map_name)) {
        logWarn(QString("Failed to set map to '%1'").arg(map_name));
        return false;
    }

    if (editor->map != nullptr && !editor->map->name.isNull()) {
        ui->mapList->setExpanded(mapListProxyModel->mapFromSource(mapListIndexes.value(editor->map->name)), false);
    }

    refreshMapScene();
    displayMapProperties();

    if (scrollTreeView) {
        // Make sure we clear the filter first so we actually have a scroll target
        mapListProxyModel->setFilterRegularExpression(QString());
        ui->mapList->setCurrentIndex(mapListProxyModel->mapFromSource(mapListIndexes.value(map_name)));
        ui->mapList->scrollTo(ui->mapList->currentIndex(), QAbstractItemView::PositionAtCenter);
    }

    ui->mapList->setExpanded(mapListProxyModel->mapFromSource(mapListIndexes.value(map_name)), true);

    showWindowTitle();

    connect(editor->map, &Map::mapNeedsRedrawing, this, &MainWindow::onMapNeedsRedrawing);

    // Swap the "currently-open" icon from the old map to the new map
    if (!userConfig.recentMap.isEmpty() && userConfig.recentMap != map_name)
        updateMapListIcon(userConfig.recentMap);
    userConfig.recentMap = map_name;
    updateMapListIcon(userConfig.recentMap);

    Scripting::cb_MapOpened(map_name);
    prefab.updatePrefabUi(editor->map);
    updateTilesetEditor();
    return true;
}

void MainWindow::redrawMapScene()
{
    if (!editor->displayMap())
        return;

    this->refreshMapScene();
}

void MainWindow::refreshMapScene()
{
    on_mainTabBar_tabBarClicked(ui->mainTabBar->currentIndex());

    ui->graphicsView_Map->setScene(editor->scene);
    ui->graphicsView_Map->setSceneRect(editor->scene->sceneRect());
    ui->graphicsView_Map->editor = editor;

    ui->graphicsView_Connections->setScene(editor->scene);
    ui->graphicsView_Connections->setSceneRect(editor->scene->sceneRect());

    ui->graphicsView_Metatiles->setScene(editor->scene_metatiles);
    //ui->graphicsView_Metatiles->setSceneRect(editor->scene_metatiles->sceneRect());
    ui->graphicsView_Metatiles->setFixedSize(editor->metatile_selector_item->pixmap().width() + 2, editor->metatile_selector_item->pixmap().height() + 2);

    ui->graphicsView_BorderMetatile->setScene(editor->scene_selected_border_metatiles);
    ui->graphicsView_BorderMetatile->setFixedSize(editor->selected_border_metatiles_item->pixmap().width() + 2, editor->selected_border_metatiles_item->pixmap().height() + 2);

    ui->graphicsView_currentMetatileSelection->setScene(editor->scene_current_metatile_selection);
    ui->graphicsView_currentMetatileSelection->setFixedSize(editor->current_metatile_selection_item->pixmap().width() + 2, editor->current_metatile_selection_item->pixmap().height() + 2);

    ui->graphicsView_Collision->setScene(editor->scene_collision_metatiles);
    //ui->graphicsView_Collision->setSceneRect(editor->scene_collision_metatiles->sceneRect());
    ui->graphicsView_Collision->setFixedSize(editor->movement_permissions_selector_item->pixmap().width() + 2, editor->movement_permissions_selector_item->pixmap().height() + 2);

    on_horizontalSlider_MetatileZoom_valueChanged(ui->horizontalSlider_MetatileZoom->value());
    on_horizontalSlider_CollisionZoom_valueChanged(ui->horizontalSlider_CollisionZoom->value());
}

void MainWindow::openWarpMap(QString map_name, int event_id, Event::Group event_group) {
    // Open the destination map.
    if (!userSetMap(map_name, true))
        return;

    // Select the target event.
    int index = event_id - Event::getIndexOffset(event_group);
    const QList<Event*> events = editor->map->events[event_group];
    if (index < events.length() && index >= 0) {
        editor->selectMapEvent(events.at(index));
    } else {
        // Can still warp to this map, but can't select the specified event
        logWarn(QString("%1 %2 doesn't exist on map '%3'").arg(Event::groupToString(event_group)).arg(event_id).arg(map_name));
    }
}

void MainWindow::displayMapProperties() {
    // Block signals to the comboboxes while they are being modified
    const QSignalBlocker b_PrimaryTileset(ui->comboBox_PrimaryTileset);
    const QSignalBlocker b_SecondaryTileset(ui->comboBox_SecondaryTileset);

    this->mapHeader->clearDisplay();
    if (!editor || !editor->map || !editor->project) {
        ui->frame_HeaderData->setEnabled(false);
        return;
    }

    ui->frame_HeaderData->setEnabled(true);
    Map *map = editor->map;

    ui->comboBox_PrimaryTileset->setCurrentText(map->layout->tileset_primary_label);
    ui->comboBox_SecondaryTileset->setCurrentText(map->layout->tileset_secondary_label);

    this->mapHeader->setMap(map);

    // Custom fields table.
    ui->tableWidget_CustomHeaderFields->blockSignals(true);
    ui->tableWidget_CustomHeaderFields->setRowCount(0);
    for (auto it = map->customHeaders.begin(); it != map->customHeaders.end(); it++)
        CustomAttributesTable::addAttribute(ui->tableWidget_CustomHeaderFields, it.key(), it.value());
    ui->tableWidget_CustomHeaderFields->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableWidget_CustomHeaderFields->blockSignals(false);
}

// Update the UI using information we've read from the user's project files.
bool MainWindow::setProjectUI() {
    Project *project = editor->project;

    this->mapHeader->setProject(project);

    // Block signals to the comboboxes while they are being modified
    const QSignalBlocker b_PrimaryTileset(ui->comboBox_PrimaryTileset);
    const QSignalBlocker b_SecondaryTileset(ui->comboBox_SecondaryTileset);
    const QSignalBlocker b_DiveMap(ui->comboBox_DiveMap);
    const QSignalBlocker b_EmergeMap(ui->comboBox_EmergeMap);

    // Set up project comboboxes
    ui->comboBox_PrimaryTileset->clear();
    ui->comboBox_PrimaryTileset->addItems(project->primaryTilesetLabels);
    ui->comboBox_SecondaryTileset->clear();
    ui->comboBox_SecondaryTileset->addItems(project->secondaryTilesetLabels);
    ui->comboBox_DiveMap->clear();
    ui->comboBox_DiveMap->addItems(project->mapNames);
    ui->comboBox_DiveMap->setClearButtonEnabled(true);
    ui->comboBox_DiveMap->setFocusedScrollingEnabled(false);
    ui->comboBox_EmergeMap->clear();
    ui->comboBox_EmergeMap->addItems(project->mapNames);
    ui->comboBox_EmergeMap->setClearButtonEnabled(true);
    ui->comboBox_EmergeMap->setFocusedScrollingEnabled(false);

    sortMapList();

    // Show/hide parts of the UI that are dependent on the user's project settings

    // Wild Encounters tab
    ui->mainTabBar->setTabEnabled(MainTab::WildPokemon, editor->project->wildEncountersLoaded);

    ui->newEventToolButton->setActionVisible(Event::Type::WeatherTrigger, projectConfig.eventWeatherTriggerEnabled);
    ui->newEventToolButton->setActionVisible(Event::Type::SecretBase, projectConfig.eventSecretBaseEnabled);
    ui->newEventToolButton->setActionVisible(Event::Type::CloneObject, projectConfig.eventCloneObjectEnabled);

    Event::setIcons();
    editor->setCollisionGraphics();
    ui->spinBox_SelectedElevation->setMaximum(Block::getMaxElevation());
    ui->spinBox_SelectedCollision->setMaximum(Block::getMaxCollision());

    return true;
}

void MainWindow::clearProjectUI() {
    // Block signals to the comboboxes while they are being modified
    const QSignalBlocker b_PrimaryTileset(ui->comboBox_PrimaryTileset);
    const QSignalBlocker b_SecondaryTileset(ui->comboBox_SecondaryTileset);
    const QSignalBlocker b_DiveMap(ui->comboBox_DiveMap);
    const QSignalBlocker b_EmergeMap(ui->comboBox_EmergeMap);
    const QSignalBlocker b_FilterBox(ui->lineEdit_filterBox);

    ui->comboBox_PrimaryTileset->clear();
    ui->comboBox_SecondaryTileset->clear();
    ui->comboBox_DiveMap->clear();
    ui->comboBox_EmergeMap->clear();
    ui->lineEdit_filterBox->clear();

    this->mapHeader->clear();

    // Clear map list
    mapListModel->clear();
    mapListIndexes.clear();
    mapGroupItemsList->clear();

    Event::clearIcons();
}

void MainWindow::sortMapList() {
    Project *project = editor->project;

    QIcon mapFolderIcon;
    mapFolderIcon.addFile(QStringLiteral(":/icons/folder_closed_map.ico"), QSize(), QIcon::Normal, QIcon::Off);
    mapFolderIcon.addFile(QStringLiteral(":/icons/folder_map.ico"), QSize(), QIcon::Normal, QIcon::On);

    QIcon folderIcon;
    folderIcon.addFile(QStringLiteral(":/icons/folder_closed.ico"), QSize(), QIcon::Normal, QIcon::Off);
    //folderIcon.addFile(QStringLiteral(":/icons/folder.ico"), QSize(), QIcon::Normal, QIcon::On);

    ui->mapList->setUpdatesEnabled(false);
    mapListModel->clear();
    mapListIndexes.clear();
    mapGroupItemsList->clear();
    QStandardItem *root = mapListModel->invisibleRootItem();

    switch (porymapConfig.mapSortOrder)
    {
        case MapSortOrder::Group:
            for (int i = 0; i < project->groupNames.length(); i++) {
                QString group_name = project->groupNames.value(i);
                QStandardItem *group = new QStandardItem;
                group->setText(group_name);
                group->setIcon(mapFolderIcon);
                group->setEditable(false);
                group->setData(group_name, Qt::UserRole);
                group->setData("map_group", MapListUserRoles::TypeRole);
                group->setData(i, MapListUserRoles::GroupRole);
                root->appendRow(group);
                mapGroupItemsList->append(group);
                QStringList names = project->groupedMapNames.value(i);
                for (int j = 0; j < names.length(); j++) {
                    QString map_name = names.value(j);
                    QStandardItem *map = createMapItem(map_name, i, j);
                    group->appendRow(map);
                    mapListIndexes.insert(map_name, map->index());
                }
            }
            break;
        case MapSortOrder::Area:
        {
            QMap<QString, int> mapsecToGroupNum;
            int row = 0;
            for (auto mapsec_value : project->mapSectionValueToName.keys()) {
                QString mapsec_name = project->mapSectionValueToName.value(mapsec_value);
                QStandardItem *mapsec = new QStandardItem;
                mapsec->setText(mapsec_name);
                mapsec->setIcon(folderIcon);
                mapsec->setEditable(false);
                mapsec->setData(mapsec_name, Qt::UserRole);
                mapsec->setData("map_sec", MapListUserRoles::TypeRole);
                root->appendRow(mapsec);
                mapGroupItemsList->append(mapsec);
                mapsecToGroupNum.insert(mapsec_name, row++);
            }
            for (int i = 0; i < project->groupNames.length(); i++) {
                QStringList names = project->groupedMapNames.value(i);
                for (int j = 0; j < names.length(); j++) {
                    QString map_name = names.value(j);
                    QStandardItem *map = createMapItem(map_name, i, j);
                    QString location = project->mapNameToMapSectionName.value(map_name);
                    QStandardItem *mapsecItem = mapGroupItemsList->at(mapsecToGroupNum[location]);
                    mapsecItem->setIcon(mapFolderIcon);
                    mapsecItem->appendRow(map);
                    mapListIndexes.insert(map_name, map->index());
                }
            }
            break;
        }
        case MapSortOrder::Layout:
        {
            QMap<QString, int> layoutIndices;
            for (int i = 0; i < project->mapLayoutsTable.length(); i++) {
                QString layoutId = project->mapLayoutsTable.value(i);
                MapLayout *layout = project->mapLayouts.value(layoutId);
                QStandardItem *layoutItem = new QStandardItem;
                layoutItem->setText(layout->name);
                layoutItem->setIcon(folderIcon);
                layoutItem->setEditable(false);
                layoutItem->setData(layout->name, Qt::UserRole);
                layoutItem->setData("map_layout", MapListUserRoles::TypeRole);
                layoutItem->setData(layout->id, MapListUserRoles::TypeRole2);
                layoutItem->setData(i, MapListUserRoles::GroupRole);
                root->appendRow(layoutItem);
                mapGroupItemsList->append(layoutItem);
                layoutIndices[layoutId] = i;
            }
            for (int i = 0; i < project->groupNames.length(); i++) {
                QStringList names = project->groupedMapNames.value(i);
                for (int j = 0; j < names.length(); j++) {
                    QString map_name = names.value(j);
                    QStandardItem *map = createMapItem(map_name, i, j);
                    QString layoutId = project->mapNameToLayoutId.value(map_name);
                    QStandardItem *layoutItem = mapGroupItemsList->at(layoutIndices.value(layoutId));
                    layoutItem->setIcon(mapFolderIcon);
                    layoutItem->appendRow(map);
                    mapListIndexes.insert(map_name, map->index());
                }
            }
            break;
        }
    }

    ui->mapList->setUpdatesEnabled(true);
    ui->mapList->repaint();
    updateMapList();
}

QStandardItem* MainWindow::createMapItem(QString mapName, int groupNum, int inGroupNum) {
    QStandardItem *map = new QStandardItem;
    map->setText(QString("[%1.%2] ").arg(groupNum).arg(inGroupNum, 2, 10, QLatin1Char('0')) + mapName);
    map->setIcon(mapIcon);
    map->setEditable(false);
    map->setData(mapName, Qt::UserRole);
    map->setData("map_name", MapListUserRoles::TypeRole);
    return map;
}

void MainWindow::onOpenMapListContextMenu(const QPoint &point)
{
    QModelIndex index = mapListProxyModel->mapToSource(ui->mapList->indexAt(point));
    if (!index.isValid()) {
        return;
    }

    QStandardItem *selectedItem = mapListModel->itemFromIndex(index);
    QVariant itemType = selectedItem->data(MapListUserRoles::TypeRole);
    if (!itemType.isValid()) {
        return;
    }

    // Build custom context menu depending on which type of item was selected (map group, map name, etc.)
    if (itemType == "map_group") {
        int groupNum = selectedItem->data(MapListUserRoles::GroupRole).toInt();
        QMenu* menu = new QMenu(this);
        QActionGroup* actions = new QActionGroup(menu);
        actions->addAction(menu->addAction("Add New Map to Group"))->setData(groupNum);
        connect(actions, &QActionGroup::triggered, this, &MainWindow::onAddNewMapToGroupClick);
        menu->exec(QCursor::pos());
    } else if (itemType == "map_sec") {
        QString secName = selectedItem->data(Qt::UserRole).toString();
        QMenu* menu = new QMenu(this);
        QActionGroup* actions = new QActionGroup(menu);
        actions->addAction(menu->addAction("Add New Map to Area"))->setData(secName);
        connect(actions, &QActionGroup::triggered, this, &MainWindow::onAddNewMapToAreaClick);
        menu->exec(QCursor::pos());
    } else if (itemType == "map_layout") {
        QString layoutId = selectedItem->data(MapListUserRoles::TypeRole2).toString();
        QMenu* menu = new QMenu(this);
        QActionGroup* actions = new QActionGroup(menu);
        actions->addAction(menu->addAction("Add New Map with Layout"))->setData(layoutId);
        connect(actions, &QActionGroup::triggered, this, &MainWindow::onAddNewMapToLayoutClick);
        menu->exec(QCursor::pos());
    }
}

void MainWindow::onAddNewMapToGroupClick(QAction* triggeredAction)
{
    openNewMapWindow();
    this->newMapDialog->init(MapSortOrder::Group, triggeredAction->data());
}

void MainWindow::onAddNewMapToAreaClick(QAction* triggeredAction)
{
    openNewMapWindow();
    this->newMapDialog->init(MapSortOrder::Area, triggeredAction->data());
}

void MainWindow::onAddNewMapToLayoutClick(QAction* triggeredAction)
{
    openNewMapWindow();
    this->newMapDialog->init(MapSortOrder::Layout, triggeredAction->data());
}

void MainWindow::onNewMapCreated() {
    int newMapGroup = this->newMapDialog->group;
    Map *newMap = this->newMapDialog->map;
    bool existingLayout = this->newMapDialog->existingLayout;
    bool importedMap = this->newMapDialog->importedMap;

    newMap = editor->project->addNewMapToGroup(newMap, newMapGroup, existingLayout, importedMap);

    logInfo(QString("Created a new map named %1.").arg(newMap->name));

    editor->project->saveMap(newMap);
    editor->project->saveAllDataStructures();

    QStandardItem* groupItem = mapGroupItemsList->at(newMapGroup);
    int numMapsInGroup = groupItem->rowCount();

    QStandardItem *newMapItem = createMapItem(newMap->name, newMapGroup, numMapsInGroup);
    groupItem->appendRow(newMapItem);
    mapListIndexes.insert(newMap->name, newMapItem->index());

    sortMapList();
    setMap(newMap->name, true);

    // Refresh any combo box that displays map names and persists between maps
    // (other combo boxes like for warp destinations are repopulated when the map changes).
    int index = this->editor->project->mapNames.indexOf(newMap->name);
    if (index >= 0) {
        const QSignalBlocker blocker1(ui->comboBox_DiveMap);
        const QSignalBlocker blocker2(ui->comboBox_EmergeMap);
        ui->comboBox_DiveMap->insertItem(index, newMap->name);
        ui->comboBox_EmergeMap->insertItem(index, newMap->name);
    }

    if (newMap->needsHealLocation) {
        newMap->needsHealLocation = false;
        this->editor->addNewEvent(Event::Type::HealLocation);
    }

    disconnect(this->newMapDialog, &NewMapDialog::applied, this, &MainWindow::onNewMapCreated);
    delete newMap;
}

void MainWindow::openNewMapWindow() {
    if (!this->newMapDefaultsSet) {
        NewMapDialog::setDefaultSettings(this->editor->project);
        this->newMapDefaultsSet = true;
    }
    if (!this->newMapDialog) {
        this->newMapDialog = new NewMapDialog(this, this->editor->project);
        connect(this->newMapDialog, &NewMapDialog::applied, this, &MainWindow::onNewMapCreated);
    }

    openSubWindow(this->newMapDialog);
}

void MainWindow::on_action_NewMap_triggered() {
    openNewMapWindow();
    this->newMapDialog->init();
}

// Insert label for newly-created tileset into sorted list of existing labels
int MainWindow::insertTilesetLabel(QStringList * list, QString label) {
    int i = 0;
    for (; i < list->length(); i++)
        if (list->at(i) > label) break;
    list->insert(i, label);
    return i;
}

void MainWindow::on_actionNew_Tileset_triggered() {
    NewTilesetDialog *createTilesetDialog = new NewTilesetDialog(editor->project, this);
    if(createTilesetDialog->exec() == QDialog::Accepted){
        if(createTilesetDialog->friendlyName.isEmpty()) {
            logError(QString("Tried to create a directory with an empty name."));
            QMessageBox msgBox(this);
            msgBox.setText("Failed to add new tileset.");
            QString message = QString("The given name was empty.");
            msgBox.setInformativeText(message);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.setIcon(QMessageBox::Icon::Critical);
            msgBox.exec();
            return;
        }
        QString fullDirectoryPath = editor->project->root + "/" + createTilesetDialog->path;
        QDir directory;
        if(directory.exists(fullDirectoryPath)) {
            logError(QString("Could not create tileset \"%1\", the folder \"%2\" already exists.").arg(createTilesetDialog->friendlyName, fullDirectoryPath));
            QMessageBox msgBox(this);
            msgBox.setText("Failed to add new tileset.");
            QString message = QString("The folder for tileset \"%1\" already exists. View porymap.log for specific errors.").arg(createTilesetDialog->friendlyName);
            msgBox.setInformativeText(message);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.setIcon(QMessageBox::Icon::Critical);
            msgBox.exec();
            return;
        }
        if (editor->project->tilesetLabelsOrdered.contains(createTilesetDialog->fullSymbolName)) {
            logError(QString("Could not create tileset \"%1\", the symbol \"%2\" already exists.").arg(createTilesetDialog->friendlyName, createTilesetDialog->fullSymbolName));
            QMessageBox msgBox(this);
            msgBox.setText("Failed to add new tileset.");
            QString message = QString("The symbol for tileset \"%1\" (\"%2\") already exists.").arg(createTilesetDialog->friendlyName, createTilesetDialog->fullSymbolName);
            msgBox.setInformativeText(message);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.setIcon(QMessageBox::Icon::Critical);
            msgBox.exec();
            return;
        }
        directory.mkdir(fullDirectoryPath);
        directory.mkdir(fullDirectoryPath + "/palettes");
        Tileset newSet;
        newSet.name = createTilesetDialog->fullSymbolName;
        newSet.tilesImagePath = fullDirectoryPath + "/tiles.png";
        newSet.metatiles_path = fullDirectoryPath + "/metatiles.bin";
        newSet.metatile_attrs_path = fullDirectoryPath + "/metatile_attributes.bin";
        newSet.is_secondary = createTilesetDialog->isSecondary;
        int numMetatiles = createTilesetDialog->isSecondary ? (Project::getNumMetatilesTotal() - Project::getNumMetatilesPrimary()) : Project::getNumMetatilesPrimary();
        QImage tilesImage(":/images/blank_tileset.png");
        editor->project->loadTilesetTiles(&newSet, tilesImage);
        int tilesPerMetatile = projectConfig.getNumTilesInMetatile();
        for(int i = 0; i < numMetatiles; ++i) {
            Metatile *mt = new Metatile();
            for(int j = 0; j < tilesPerMetatile; ++j){
                Tile tile = Tile();
                if (createTilesetDialog->checkerboardFill) {
                    // Create a checkerboard-style dummy tileset
                    if (((i / 8) % 2) == 0)
                        tile.tileId = ((i % 2) == 0) ? 1 : 2;
                    else
                        tile.tileId = ((i % 2) == 1) ? 1 : 2;
                }
                mt->tiles.append(tile);
            }
            newSet.metatiles.append(mt);
        }
        for(int i = 0; i < 16; ++i) {
            QList<QRgb> currentPal;
            for(int i = 0; i < 16;++i) {
                currentPal.append(qRgb(0,0,0));
            }
            newSet.palettes.append(currentPal);
            newSet.palettePreviews.append(currentPal);
            QString fileName = QString("%1.pal").arg(i, 2, 10, QLatin1Char('0'));
            newSet.palettePaths.append(fullDirectoryPath+"/palettes/" + fileName);
        }
        newSet.palettes[0][1] = qRgb(255,0,255);
        newSet.palettePreviews[0][1] = qRgb(255,0,255);
        exportIndexed4BPPPng(newSet.tilesImage, newSet.tilesImagePath);
        editor->project->saveTilesetMetatiles(&newSet);
        editor->project->saveTilesetMetatileAttributes(&newSet);
        editor->project->saveTilesetPalettes(&newSet);

        //append to tileset specific files
        newSet.appendToHeaders(editor->project->root, createTilesetDialog->friendlyName, editor->project->usingAsmTilesets);
        newSet.appendToGraphics(editor->project->root, createTilesetDialog->friendlyName, editor->project->usingAsmTilesets);
        newSet.appendToMetatiles(editor->project->root, createTilesetDialog->friendlyName, editor->project->usingAsmTilesets);

        if (!createTilesetDialog->isSecondary) {
            int index = insertTilesetLabel(&editor->project->primaryTilesetLabels, createTilesetDialog->fullSymbolName);
            this->ui->comboBox_PrimaryTileset->insertItem(index, createTilesetDialog->fullSymbolName);
        } else {
            int index = insertTilesetLabel(&editor->project->secondaryTilesetLabels, createTilesetDialog->fullSymbolName);
            this->ui->comboBox_SecondaryTileset->insertItem(index, createTilesetDialog->fullSymbolName);
        }
        insertTilesetLabel(&editor->project->tilesetLabelsOrdered, createTilesetDialog->fullSymbolName);

        QMessageBox msgBox(this);
        msgBox.setText("Successfully created tileset.");
        QString message = QString("Tileset \"%1\" was created successfully.").arg(createTilesetDialog->friendlyName);
        msgBox.setInformativeText(message);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Icon::Information);
        msgBox.exec();
    }
}

void MainWindow::updateTilesetEditor() {
    if (this->tilesetEditor) {
        this->tilesetEditor->update(
            this->editor->map,
            editor->ui->comboBox_PrimaryTileset->currentText(),
            editor->ui->comboBox_SecondaryTileset->currentText()
        );
    }
}

double MainWindow::getMetatilesZoomScale() {
    return pow(3.0, static_cast<double>(porymapConfig.metatilesZoom - 30) / 30.0);
}

void MainWindow::redrawMetatileSelection() {
    QSize size(editor->current_metatile_selection_item->pixmap().width(), editor->current_metatile_selection_item->pixmap().height());
    ui->graphicsView_currentMetatileSelection->setSceneRect(0, 0, size.width(), size.height());

    auto scale = getMetatilesZoomScale();
    QTransform transform;
    transform.scale(scale, scale);
    size *= scale;

    ui->graphicsView_currentMetatileSelection->setTransform(transform);
    ui->graphicsView_currentMetatileSelection->setFixedSize(size.width() + 2, size.height() + 2);
    ui->scrollAreaWidgetContents_SelectedMetatiles->adjustSize();
}

void MainWindow::scrollMetatileSelectorToSelection() {
    // Internal selections or 1x1 external selections can be scrolled to
    if (!editor->metatile_selector_item->isInternalSelection() && editor->metatile_selector_item->getSelectionDimensions() != QPoint(1, 1))
        return;

    MetatileSelection selection = editor->metatile_selector_item->getMetatileSelection();
    if (selection.metatileItems.isEmpty())
        return;

    QPoint pos = editor->metatile_selector_item->getMetatileIdCoordsOnWidget(selection.metatileItems.first().metatileId);
    QPoint size = editor->metatile_selector_item->getSelectionDimensions();
    pos += QPoint(size.x() - 1, size.y() - 1) * 16 / 2; // We want to focus on the center of the whole selection
    pos *= getMetatilesZoomScale();

    auto viewport = ui->scrollArea_MetatileSelector->viewport();
    ui->scrollArea_MetatileSelector->ensureVisible(pos.x(), pos.y(), viewport->width() / 2, viewport->height() / 2);
}

void MainWindow::currentMetatilesSelectionChanged() {
    redrawMetatileSelection();
    if (this->tilesetEditor) {
        MetatileSelection selection = editor->metatile_selector_item->getMetatileSelection();
        this->tilesetEditor->selectMetatile(selection.metatileItems.first().metatileId);
    }

    // Don't scroll to internal selections here, it will disrupt the user while they make their selection.
    if (!editor->metatile_selector_item->isInternalSelection())
        scrollMetatileSelectorToSelection();
}

void MainWindow::on_mapList_activated(const QModelIndex &index)
{
    QVariant data = index.data(Qt::UserRole);
    if (index.data(MapListUserRoles::TypeRole) == "map_name" && !data.isNull())
        userSetMap(data.toString());
}

void MainWindow::updateMapListIcon(const QString &mapName) {
    if (!editor->project || !editor->project->mapCache.contains(mapName))
        return;

    QStandardItem *item = mapListModel->itemFromIndex(mapListIndexes.value(mapName));
    if (!item)
        return;

    static const QIcon mapEditedIcon = QIcon(QStringLiteral(":/icons/map_edited.ico"));
    static const QIcon mapOpenedIcon = QIcon(QStringLiteral(":/icons/map_opened.ico"));

    if (editor->map && editor->map->name == mapName) {
        item->setIcon(mapOpenedIcon);
    } else if (editor->project->mapCache.value(mapName)->hasUnsavedChanges()) {
        item->setIcon(mapEditedIcon);
    } else {
        item->setIcon(mapIcon);
    }
}

void MainWindow::updateMapList() {
    QList<QModelIndex> list;
    list.append(QModelIndex());
    while (list.length()) {
        QModelIndex parent = list.takeFirst();
        for (int i = 0; i < mapListModel->rowCount(parent); i++) {
            QModelIndex index = mapListModel->index(i, 0, parent);
            if (mapListModel->hasChildren(index)) {
                list.append(index);
            }
            QVariant data = index.data(Qt::UserRole);
            if (!data.isNull()) {
                updateMapListIcon(data.toString());
            }
        }
    }
}

void MainWindow::on_action_Save_Project_triggered() {
    editor->saveProject();
    updateMapList();
    showWindowTitle();
    saveGlobalConfigs();
}

void MainWindow::on_action_Save_triggered() {
    editor->save();
    if (editor->map)
        updateMapListIcon(editor->map->name);
    showWindowTitle();
    saveGlobalConfigs();
}

void MainWindow::duplicate() {
    editor->duplicateSelectedEvents();
}

void MainWindow::copy() {
    auto focused = QApplication::focusWidget();
    if (focused) {
        QString objectName = focused->objectName();
        if (objectName == "graphicsView_currentMetatileSelection") {
            // copy the current metatile selection as json data
            OrderedJson::object copyObject;
            copyObject["object"] = "metatile_selection";
            OrderedJson::array metatiles;
            MetatileSelection selection = editor->metatile_selector_item->getMetatileSelection();
            for (auto item : selection.metatileItems) {
                metatiles.append(static_cast<int>(item.metatileId));
            }
            OrderedJson::array collisions;
            if (selection.hasCollision) {
                for (auto item : selection.collisionItems) {
                    OrderedJson::object collision;
                    collision["collision"] = item.collision;
                    collision["elevation"] = item.elevation;
                    collisions.append(collision);
                }
            }
            if (collisions.length() != metatiles.length()) {
                // fill in collisions
                collisions.clear();
                for (int i = 0; i < metatiles.length(); i++) {
                    OrderedJson::object collision;
                    collision["collision"] = projectConfig.defaultCollision;
                    collision["elevation"] = projectConfig.defaultElevation;
                    collisions.append(collision);
                }
            }
            copyObject["metatile_selection"] = metatiles;
            copyObject["collision_selection"] = collisions;
            copyObject["width"] = editor->metatile_selector_item->getSelectionDimensions().x();
            copyObject["height"] = editor->metatile_selector_item->getSelectionDimensions().y();
            setClipboardData(copyObject);
            logInfo("Copied metatile selection to clipboard");
        }
        else if (objectName == "graphicsView_Map") {
            // which tab are we in?
            switch (ui->mainTabBar->currentIndex())
            {
            default:
                break;
            case MainTab::Map:
            {
                // copy the map image
                QPixmap pixmap = editor->map ? editor->map->render(true) : QPixmap();
                setClipboardData(pixmap.toImage());
                logInfo("Copied current map image to clipboard");
                break;
            }
            case MainTab::Events:
            {
                if (!editor || !editor->project || !editor->map) break;

                // copy the currently selected event(s) as a json object
                OrderedJson::object copyObject;
                copyObject["object"] = "events";

                OrderedJson::array eventsArray;
                for (const auto &event : editor->selectedEventsByMap[editor->map->name]) {
                    OrderedJson::object eventContainer;
                    eventContainer["event_type"] = event->typeString();
                    OrderedJson::object eventJson = event->buildEventJson(editor->project);
                    eventContainer["event"] = eventJson;
                    eventsArray.append(eventContainer);
                }

                if (!eventsArray.isEmpty()) {
                    copyObject["events"] = eventsArray;
                    setClipboardData(copyObject);
                    logInfo("Copied currently selected events to clipboard");
                }
                break;
            }
            }
        }
        else if (this->ui->mainTabBar->currentIndex() == MainTab::WildPokemon) {
            QWidget *w = this->ui->stackedWidget_WildMons->currentWidget();
            if (w) {
                MonTabWidget *mtw = static_cast<MonTabWidget *>(w);
                mtw->copy(mtw->currentIndex());
            }
        }
    }
}

void MainWindow::setClipboardData(OrderedJson::object object) {
    QClipboard *clipboard = QGuiApplication::clipboard();
    QString newText;
    int indent = 0;
    object["application"] = "porymap";
    OrderedJson data(object);
    data.dump(newText, &indent);
    clipboard->setText(newText);
}

void MainWindow::setClipboardData(QImage image) {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setImage(image);
}

void MainWindow::paste() {
    if (!editor || !editor->project || !editor->map) return;

    QClipboard *clipboard = QGuiApplication::clipboard();
    QString clipboardText(clipboard->text());

    if (ui->mainTabBar->currentIndex() == MainTab::WildPokemon) {
        QWidget *w = this->ui->stackedWidget_WildMons->currentWidget();
        if (w) {
            w->setFocus();
            MonTabWidget *mtw = static_cast<MonTabWidget *>(w);
            mtw->paste(mtw->currentIndex());
        }
    }
    else if (!clipboardText.isEmpty()) {
        // we only can paste json text
        // so, check if clipboard text is valid json
        QJsonDocument pasteJsonDoc = QJsonDocument::fromJson(clipboardText.toUtf8());

        // test empty
        QJsonObject pasteObject = pasteJsonDoc.object();

        //OrderedJson::object pasteObject = pasteJson.object_items();
        if (pasteObject["application"].toString() != "porymap") {
            return;
        }

        logInfo("Attempting to paste from JSON in clipboard");

        switch (ui->mainTabBar->currentIndex())
            {
            default:
                break;
            case MainTab::Map:
            {
                // can only paste currently selected metatiles on this tab
                if (pasteObject["object"].toString() != "metatile_selection") {
                    return;
                }
                QJsonArray metatilesArray = pasteObject["metatile_selection"].toArray();
                QJsonArray collisionsArray = pasteObject["collision_selection"].toArray();
                int width = ParseUtil::jsonToInt(pasteObject["width"]);
                int height = ParseUtil::jsonToInt(pasteObject["height"]);
                QList<uint16_t> metatiles;
                QList<QPair<uint16_t, uint16_t>> collisions;
                for (auto tile : metatilesArray) {
                    metatiles.append(static_cast<uint16_t>(tile.toInt()));
                }
                for (QJsonValue collision : collisionsArray) {
                    collisions.append({static_cast<uint16_t>(collision["collision"].toInt()), static_cast<uint16_t>(collision["elevation"].toInt())});
                }
                editor->metatile_selector_item->setExternalSelection(width, height, metatiles, collisions);
                break;
            }
            case MainTab::Events:
            {
                // can only paste events to this tab
                if (pasteObject["object"].toString() != "events") {
                    return;
                }

                QList<Event *> newEvents;

                QJsonArray events = pasteObject["events"].toArray();
                for (QJsonValue event : events) {
                    // paste the event to the map
                    const QString typeString = event["event_type"].toString();
                    Event::Type type = Event::typeFromString(typeString);

                    if (this->editor->eventLimitReached(type)) {
                        logWarn(QString("Cannot paste event, the limit for type '%1' has been reached.").arg(typeString));
                        continue;
                    }
                    if (type == Event::Type::HealLocation && !porymapConfig.allowHealLocationDeleting) {
                        // Can't freely add Heal Locations if deleting them is not enabled.
                        logWarn(QString("Cannot paste event, adding Heal Locations is disabled.").arg(typeString));
                        continue;
                    }

                    Event *pasteEvent = Event::create(type);
                    if (!pasteEvent)
                        continue;

                    pasteEvent->loadFromJson(event["event"].toObject(), this->editor->project);
                    pasteEvent->setMap(this->editor->map);
                    newEvents.append(pasteEvent);
                }

                if (!newEvents.empty()) {
                    editor->map->editHistory.push(new EventPaste(this->editor, editor->map, newEvents));
                }

                break;
            }
        }
    }
}

void MainWindow::on_mapViewTab_tabBarClicked(int index)
{
    int oldIndex = ui->mapViewTab->currentIndex();
    ui->mapViewTab->setCurrentIndex(index);
    if (index != oldIndex)
        Scripting::cb_MapViewTabChanged(oldIndex, index);

    if (index == MapViewTab::Metatiles) {
        editor->setEditingMap();
    } else if (index == MapViewTab::Collision) {
        editor->setEditingCollision();
    } else if (index == MapViewTab::Prefabs) {
        editor->setEditingMap();
        if (projectConfig.prefabFilepath.isEmpty() && !projectConfig.prefabImportPrompted) {
            // User hasn't set up prefabs and hasn't been prompted before.
            // Ask if they'd like to import the default prefabs file.
            if (prefab.tryImportDefaultPrefabs(this, projectConfig.baseGameVersion))
                prefab.updatePrefabUi(this->editor->map);
        } 
    }
    editor->setCursorRectVisible(false);
}

void MainWindow::on_mainTabBar_tabBarClicked(int index)
{
    int oldIndex = ui->mainTabBar->currentIndex();
    ui->mainTabBar->setCurrentIndex(index);
    if (index != oldIndex)
        Scripting::cb_MainTabChanged(oldIndex, index);

    static const QMap<int, int> tabIndexToStackIndex = {
        {MainTab::Map, 0},
        {MainTab::Events, 0},
        {MainTab::Header, 1},
        {MainTab::Connections, 2},
        {MainTab::WildPokemon, 3},
    };
    ui->mainStackedWidget->setCurrentIndex(tabIndexToStackIndex.value(index));

    if (index == MainTab::Map) {
        ui->stackedWidget_MapEvents->setCurrentIndex(0);
        on_mapViewTab_tabBarClicked(ui->mapViewTab->currentIndex());
        clickToolButtonFromEditMode(editor->map_edit_mode);
    } else if (index == MainTab::Events) {
        ui->stackedWidget_MapEvents->setCurrentIndex(1);
        editor->setEditingEvents();
        clickToolButtonFromEditMode(editor->obj_edit_mode);
    } else if (index == MainTab::Connections) {
        editor->setEditingConnections();
        // Stop the Dive/Emerge combo boxes from getting the initial focus
        ui->graphicsView_Connections->setFocus();
    }
    if (index != MainTab::WildPokemon) {
        if (editor->project && editor->project->wildEncountersLoaded)
            editor->saveEncounterTabData();
    }
    if (index != MainTab::Events) {
        editor->map_ruler->setEnabled(false);
    }
}

void MainWindow::on_actionZoom_In_triggered() {
    editor->scaleMapView(1);
}

void MainWindow::on_actionZoom_Out_triggered() {
    editor->scaleMapView(-1);
}

void MainWindow::on_actionBetter_Cursors_triggered() {
    porymapConfig.prettyCursors = ui->actionBetter_Cursors->isChecked();
    this->editor->settings->betterCursors = ui->actionBetter_Cursors->isChecked();
}

void MainWindow::on_actionPlayer_View_Rectangle_triggered()
{
    bool enabled = ui->actionPlayer_View_Rectangle->isChecked();
    porymapConfig.showPlayerView = enabled;
    this->editor->settings->playerViewRectEnabled = enabled;
    if ((this->editor->map_item && this->editor->map_item->has_mouse)
     || (this->editor->collision_item && this->editor->collision_item->has_mouse)) {
        this->editor->playerViewRect->setVisible(enabled);
        ui->graphicsView_Map->scene()->update();
    }
}

void MainWindow::on_actionCursor_Tile_Outline_triggered()
{
    bool enabled = ui->actionCursor_Tile_Outline->isChecked();
    porymapConfig.showCursorTile = enabled;
    this->editor->settings->cursorTileRectEnabled = enabled;
    if ((this->editor->map_item && this->editor->map_item->has_mouse)
     || (this->editor->collision_item && this->editor->collision_item->has_mouse)) {
        this->editor->cursorMapTileRect->setVisible(enabled && this->editor->cursorMapTileRect->getActive());
        ui->graphicsView_Map->scene()->update();
    }
}

void MainWindow::on_actionShow_Grid_triggered() {
    this->editor->toggleGrid(ui->actionShow_Grid->isChecked());
}

void MainWindow::on_actionGrid_Settings_triggered() {
    if (!this->gridSettingsDialog) {
        this->gridSettingsDialog = new GridSettingsDialog(&this->editor->gridSettings, this);
        connect(this->gridSettingsDialog, &GridSettingsDialog::changedGridSettings, this->editor, &Editor::updateMapGrid);
    }
    openSubWindow(this->gridSettingsDialog);
}

void MainWindow::on_actionShortcuts_triggered()
{
    if (!shortcutsEditor)
        initShortcutsEditor();

    openSubWindow(shortcutsEditor);
}

void MainWindow::initShortcutsEditor() {
    shortcutsEditor = new ShortcutsEditor(this);
    connect(shortcutsEditor, &ShortcutsEditor::shortcutsSaved,
            this, &MainWindow::applyUserShortcuts);

    connectSubEditorsToShortcutsEditor();

    shortcutsEditor->setShortcutableObjects(shortcutableObjects());
}

void MainWindow::connectSubEditorsToShortcutsEditor() {
    /* Initialize sub-editors so that their children are added to MainWindow's object tree and will
     * be returned by shortcutableObjects() to be passed to ShortcutsEditor. */
    if (!tilesetEditor)
        initTilesetEditor();
    connect(shortcutsEditor, &ShortcutsEditor::shortcutsSaved,
            tilesetEditor, &TilesetEditor::applyUserShortcuts);

    if (!regionMapEditor)
        initRegionMapEditor(true);
    if (regionMapEditor)
        connect(shortcutsEditor, &ShortcutsEditor::shortcutsSaved,
                regionMapEditor, &RegionMapEditor::applyUserShortcuts);

    if (!customScriptsEditor)
        initCustomScriptsEditor();
    connect(shortcutsEditor, &ShortcutsEditor::shortcutsSaved,
            customScriptsEditor, &CustomScriptsEditor::applyUserShortcuts);
}

void MainWindow::on_actionPencil_triggered()
{
    on_toolButton_Paint_clicked();
}

void MainWindow::on_actionPointer_triggered()
{
    on_toolButton_Select_clicked();
}

void MainWindow::on_actionFlood_Fill_triggered()
{
    on_toolButton_Fill_clicked();
}

void MainWindow::on_actionEyedropper_triggered()
{
    on_toolButton_Dropper_clicked();
}

void MainWindow::on_actionMove_triggered()
{
    on_toolButton_Move_clicked();
}

void MainWindow::on_actionMap_Shift_triggered()
{
    on_toolButton_Shift_clicked();
}

void MainWindow::resetMapViewScale() {
    editor->scaleMapView(0);
}

struct EventTabUI {
    QWidget *tab;
    QScrollArea *scrollArea;
    QWidget *contents;
};

void MainWindow::refreshEventsTab(Event::Group eventGroup) {
    // Map the event groups to their corresponding widgets in the UI.
    static const QMap<Event::Group, EventTabUI> groupToUI = {
        {Event::Group::Object, {ui->tab_Objects,       ui->scrollArea_Objects,       ui->scrollAreaWidgetContents_Objects}},
        {Event::Group::Warp,   {ui->tab_Warps,         ui->scrollArea_Warps,         ui->scrollAreaWidgetContents_Warps}},
        {Event::Group::Coord,  {ui->tab_Triggers,      ui->scrollArea_Triggers,      ui->scrollAreaWidgetContents_Triggers}},
        {Event::Group::Bg,     {ui->tab_BGs,           ui->scrollArea_BGs,           ui->scrollAreaWidgetContents_BGs}},
        {Event::Group::Heal,   {ui->tab_HealLocations, ui->scrollArea_HealLocations, ui->scrollAreaWidgetContents_HealLocations}},
        {Event::Group::None,   {ui->tab_Selected,      ui->scrollArea_Selected,      ui->scrollAreaWidgetContents_Selected}},
    };

    // Get the events to populate this tab with
    const QList<Event*> *events = nullptr;
    if (editor->map) {
        if (eventGroup != Event::Group::None) {
            // Show all the map's events that belong to this group
            events = &editor->map->events[eventGroup];
        } else {
            // Show the selected events for this map
            events = &editor->selectedEventsByMap[editor->map->name];
        }
    }

    const struct EventTabUI tabUI = groupToUI.value(eventGroup);
    if (!tabUI.tab || !tabUI.scrollArea || !tabUI.contents)
        return;

    int tabIndex = ui->tabWidget_EventType->indexOf(tabUI.tab);

    if (!events || events->length() == 0) {
        // Tab has no events, hide it. No further updates needed if tab is hidden.
        ui->tabWidget_EventType->setTabVisible(tabIndex, false);
        // TODO: Do we need to handle selecting the next tab (if there is one)?
        // TODO: Hidden tabs are popping in and out weirdly. Disable them, keeping them always visible? Selected tab needs to disappear, at least.
        return;
    }

    ui->tabWidget_EventType->setTabVisible(tabIndex, true);

    // Create the event frames for the targeted events. If they've already been created we just repopulate it.
    QList<QFrame *> frames;
    for (const auto &event : *events) {
        EventFrame *eventFrame = event->createEventFrame();
        eventFrame->populate(this->editor->project);
        eventFrame->initialize();
        eventFrame->connectSignals(this);
        frames.append(eventFrame);
    }

    // Delete the old layout
    if (tabUI.contents->layout() && tabUI.contents->children().length()) {
        for (const auto &frame : tabUI.contents->findChildren<EventFrame *>()) {
            if (!frames.contains(frame))
                frame->hide();
        }
        delete tabUI.contents->layout();
    }

    // Construct a layout for the event frames, then display it.
    QVBoxLayout *layout = new QVBoxLayout;
    tabUI.contents->setLayout(layout);
    tabUI.scrollArea->setWidgetResizable(true);
    tabUI.scrollArea->setWidget(tabUI.contents);

    for (const auto &frame : frames) {
        layout->addWidget(frame);
    }
    layout->addStretch(1);
    tabUI.scrollArea->adjustSize();

    // Show the frames after the vertical spacer is added to avoid visual jank
    // where the frame would stretch to the bottom of the layout.
    for (const auto &frame : frames) {
        frame->show();
    }
}

void MainWindow::clearEventsPanel() {
    ui->tabWidget_EventType->hide();
    ui->label_NoEvents->show();
}

// TODO: Creating a first event of a group doesn't display the new tab. Likewise in reverse for deleting.
// TODO: It should be possible to start an event selection by interacting with an event panel
void MainWindow::refreshEventsPanel() {
    if (!editor->map || !editor->map->hasEvents()) {
        // Not displaying map, or map has no events.
        // TODO: Can this potentially display the contents of an old tab?
        clearEventsPanel();
        return;
    }

    refreshEventsTab(Event::Group::Object);
    refreshEventsTab(Event::Group::Warp);
    refreshEventsTab(Event::Group::Coord);
    refreshEventsTab(Event::Group::Bg);
    refreshEventsTab(Event::Group::Heal);
    refreshSelectedEventsTab();

    ui->label_NoEvents->hide();
    ui->tabWidget_EventType->show();
}

// TODO: We can still try to append frames, rather than rebuild this tab every time the selection changes.
void MainWindow::refreshSelectedEventsTab() {
    refreshEventsTab(Event::Group::None);

    if (editor->map) {
        const QList<Event*> *selectedEvents = &editor->selectedEventsByMap[editor->map->name];
        if (selectedEvents->length() > 0) {
            // Switch to the selected events tab
            ui->tabWidget_EventType->setCurrentIndex(ui->tabWidget_EventType->indexOf(ui->tab_Selected));

            // Update the New Event button to show the type of the most recently-selected event.
            ui->newEventToolButton->setDefaultAction(selectedEvents->constLast()->getEventType());
        }
    }
}

void MainWindow::on_actionDive_Emerge_Map_triggered() {
    setDivingMapsVisible(ui->actionDive_Emerge_Map->isChecked());
}

void MainWindow::on_groupBox_DiveMapOpacity_toggled(bool on) {
    setDivingMapsVisible(on);
}

void MainWindow::setDivingMapsVisible(bool visible) {
    // Qt doesn't change the style of disabled sliders, so we do it ourselves
    QString stylesheet = visible ? "" : "QSlider::groove:horizontal {border: 1px solid #999999; border-radius: 3px; height: 2px; background: #B1B1B1;}"
                                        "QSlider::handle:horizontal {border: 1px solid #444444; border-radius: 3px; width: 10px; height: 9px; margin: -5px -1px; background: #5C5C5C; }";
    ui->slider_DiveEmergeMapOpacity->setStyleSheet(stylesheet);
    ui->slider_DiveMapOpacity->setStyleSheet(stylesheet);
    ui->slider_EmergeMapOpacity->setStyleSheet(stylesheet);

    // Sync UI toggle elements
    const QSignalBlocker blocker1(ui->groupBox_DiveMapOpacity);
    const QSignalBlocker blocker2(ui->actionDive_Emerge_Map);
    ui->groupBox_DiveMapOpacity->setChecked(visible);
    ui->actionDive_Emerge_Map->setChecked(visible);

    porymapConfig.showDiveEmergeMaps = visible;

    if (visible) {
        // We skip rendering diving maps if this setting is not enabled,
        // so when we enable it we need to make sure they've rendered.
        this->editor->renderDivingConnections();
    }
    this->editor->updateDivingMapsVisibility();
}

// Normally a map only has either a Dive map connection or an Emerge map connection,
// in which case we only show a single opacity slider to modify the one in use.
// If a user has both connections we show two separate opacity sliders so they can
// modify them independently.
void MainWindow::on_slider_DiveEmergeMapOpacity_valueChanged(int value) {
    porymapConfig.diveEmergeMapOpacity = value;
    this->editor->updateDivingMapsVisibility();
}

void MainWindow::on_slider_DiveMapOpacity_valueChanged(int value) {
    porymapConfig.diveMapOpacity = value;
    this->editor->updateDivingMapsVisibility();
}

void MainWindow::on_slider_EmergeMapOpacity_valueChanged(int value) {
    porymapConfig.emergeMapOpacity = value;
    this->editor->updateDivingMapsVisibility();
}

void MainWindow::on_horizontalSlider_CollisionTransparency_valueChanged(int value) {
    this->editor->collisionOpacity = static_cast<qreal>(value) / 100;
    porymapConfig.collisionOpacity = value;
    this->editor->collision_item->draw(true);
}

void MainWindow::onDeleteKeyPressed() {
    if (!editor)
        return;

    auto tab = ui->mainTabBar->currentIndex();
    if (tab == MainTab::Events) {
        editor->deleteSelectedMapEvents();
    } else if (tab == MainTab::Connections) {
        editor->removeSelectedConnection();
    }
}

void MainWindow::on_toolButton_Paint_clicked()
{
    if (ui->mainTabBar->currentIndex() == MainTab::Map)
        editor->map_edit_mode = "paint";
    else
        editor->obj_edit_mode = "paint";

    editor->settings->mapCursor = QCursor(QPixmap(":/icons/pencil_cursor.ico"), 10, 10);

    if (ui->mapViewTab->currentIndex() != MapViewTab::Collision)
        editor->cursorMapTileRect->stopSingleTileMode();

    ui->graphicsView_Map->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->graphicsView_Map->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QScroller::ungrabGesture(ui->graphicsView_Map);
    ui->graphicsView_Map->setViewportUpdateMode(QGraphicsView::ViewportUpdateMode::MinimalViewportUpdate);
    ui->graphicsView_Map->setFocus();

    checkToolButtons();
}

void MainWindow::on_toolButton_Select_clicked()
{
    if (ui->mainTabBar->currentIndex() == MainTab::Map)
        editor->map_edit_mode = "select";
    else
        editor->obj_edit_mode = "select";

    editor->settings->mapCursor = QCursor();
    editor->cursorMapTileRect->setSingleTileMode();

    ui->graphicsView_Map->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->graphicsView_Map->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QScroller::ungrabGesture(ui->graphicsView_Map);
    ui->graphicsView_Map->setViewportUpdateMode(QGraphicsView::ViewportUpdateMode::MinimalViewportUpdate);
    ui->graphicsView_Map->setFocus();

    checkToolButtons();
}

void MainWindow::on_toolButton_Fill_clicked()
{
    if (ui->mainTabBar->currentIndex() == MainTab::Map)
        editor->map_edit_mode = "fill";
    else
        editor->obj_edit_mode = "fill";

    editor->settings->mapCursor = QCursor(QPixmap(":/icons/fill_color_cursor.ico"), 10, 10);
    editor->cursorMapTileRect->setSingleTileMode();

    ui->graphicsView_Map->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->graphicsView_Map->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QScroller::ungrabGesture(ui->graphicsView_Map);
    ui->graphicsView_Map->setViewportUpdateMode(QGraphicsView::ViewportUpdateMode::MinimalViewportUpdate);
    ui->graphicsView_Map->setFocus();

    checkToolButtons();
}

void MainWindow::on_toolButton_Dropper_clicked()
{
    if (ui->mainTabBar->currentIndex() == MainTab::Map)
        editor->map_edit_mode = "pick";
    else
        editor->obj_edit_mode = "pick";

    editor->settings->mapCursor = QCursor(QPixmap(":/icons/pipette_cursor.ico"), 10, 10);
    editor->cursorMapTileRect->setSingleTileMode();

    ui->graphicsView_Map->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->graphicsView_Map->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QScroller::ungrabGesture(ui->graphicsView_Map);
    ui->graphicsView_Map->setViewportUpdateMode(QGraphicsView::ViewportUpdateMode::MinimalViewportUpdate);
    ui->graphicsView_Map->setFocus();

    checkToolButtons();
}

void MainWindow::on_toolButton_Move_clicked()
{
    if (ui->mainTabBar->currentIndex() == MainTab::Map)
        editor->map_edit_mode = "move";
    else
        editor->obj_edit_mode = "move";

    editor->settings->mapCursor = QCursor(QPixmap(":/icons/move.ico"), 7, 7);
    editor->cursorMapTileRect->setSingleTileMode();

    ui->graphicsView_Map->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView_Map->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(ui->graphicsView_Map, QScroller::LeftMouseButtonGesture);
    ui->graphicsView_Map->setViewportUpdateMode(QGraphicsView::ViewportUpdateMode::FullViewportUpdate);
    ui->graphicsView_Map->setFocus();

    checkToolButtons();
}

void MainWindow::on_toolButton_Shift_clicked()
{
    if (ui->mainTabBar->currentIndex() == MainTab::Map)
        editor->map_edit_mode = "shift";
    else
        editor->obj_edit_mode = "shift";

    editor->settings->mapCursor = QCursor(QPixmap(":/icons/shift_cursor.ico"), 10, 10);
    editor->cursorMapTileRect->setSingleTileMode();

    ui->graphicsView_Map->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->graphicsView_Map->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QScroller::ungrabGesture(ui->graphicsView_Map);
    ui->graphicsView_Map->setViewportUpdateMode(QGraphicsView::ViewportUpdateMode::MinimalViewportUpdate);
    ui->graphicsView_Map->setFocus();

    checkToolButtons();
}

void MainWindow::checkToolButtons() {
    QString edit_mode;
    if (ui->mainTabBar->currentIndex() == MainTab::Map) {
        edit_mode = editor->map_edit_mode;
    } else {
        edit_mode = editor->obj_edit_mode;
        if (edit_mode == "select" && editor->map_ruler)
            editor->map_ruler->setEnabled(true);
        else if (editor->map_ruler)
            editor->map_ruler->setEnabled(false);
    }

    ui->toolButton_Paint->setChecked(edit_mode == "paint");
    ui->toolButton_Select->setChecked(edit_mode == "select");
    ui->toolButton_Fill->setChecked(edit_mode == "fill");
    ui->toolButton_Dropper->setChecked(edit_mode == "pick");
    ui->toolButton_Move->setChecked(edit_mode == "move");
    ui->toolButton_Shift->setChecked(edit_mode == "shift");
}

void MainWindow::clickToolButtonFromEditMode(QString editMode) {
    if (editMode == "paint") {
        on_toolButton_Paint_clicked();
    } else if (editMode == "select") {
        on_toolButton_Select_clicked();
    } else if (editMode == "fill") {
        on_toolButton_Fill_clicked();
    } else if (editMode == "pick") {
        on_toolButton_Dropper_clicked();
    } else if (editMode == "move") {
        on_toolButton_Move_clicked();
    } else if (editMode == "shift") {
        on_toolButton_Shift_clicked();
    }
}

void MainWindow::onOpenConnectedMap(MapConnection *connection) {
    if (!connection)
        return;
    if (userSetMap(connection->targetMapName(), true))
        editor->setSelectedConnection(connection->findMirror());
}

void MainWindow::onMapNeedsRedrawing() {
    redrawMapScene();
}

void MainWindow::onMapCacheCleared() {
    editor->map = nullptr;
}

void MainWindow::onMapLoaded(Map *map) {
    connect(map, &Map::modified, [this, map] { this->markMapEdited(map); });
}

void MainWindow::onTilesetsSaved(QString primaryTilesetLabel, QString secondaryTilesetLabel) {
    // If saved tilesets are currently in-use, update them and redraw
    // Otherwise overwrite the cache for the saved tileset
    bool updated = false;
    if (primaryTilesetLabel == this->editor->map->layout->tileset_primary_label) {
        this->editor->updatePrimaryTileset(primaryTilesetLabel, true);
        Scripting::cb_TilesetUpdated(primaryTilesetLabel);
        updated = true;
    } else {
        this->editor->project->getTileset(primaryTilesetLabel, true);
    }
    if (secondaryTilesetLabel == this->editor->map->layout->tileset_secondary_label)  {
        this->editor->updateSecondaryTileset(secondaryTilesetLabel, true);
        Scripting::cb_TilesetUpdated(secondaryTilesetLabel);
        updated = true;
    } else {
        this->editor->project->getTileset(secondaryTilesetLabel, true);
    }
    if (updated)
        redrawMapScene();
}

void MainWindow::onMapRulerStatusChanged(const QString &status) {
    if (status.isEmpty()) {
        label_MapRulerStatus->hide();
    } else if (label_MapRulerStatus->parentWidget()) {
        label_MapRulerStatus->setText(status);
        label_MapRulerStatus->adjustSize();
        label_MapRulerStatus->show();
        label_MapRulerStatus->move(label_MapRulerStatus->parentWidget()->mapToGlobal(QPoint(6, 6)));
    }
}

void MainWindow::moveEvent(QMoveEvent *event) {
    QMainWindow::moveEvent(event);
    if (label_MapRulerStatus && label_MapRulerStatus->isVisible() && label_MapRulerStatus->parentWidget())
        label_MapRulerStatus->move(label_MapRulerStatus->parentWidget()->mapToGlobal(QPoint(6, 6)));
}

void MainWindow::on_action_Export_Map_Image_triggered() {
    showExportMapImageWindow(ImageExporterMode::Normal);
}

void MainWindow::on_actionExport_Stitched_Map_Image_triggered() {
    showExportMapImageWindow(ImageExporterMode::Stitch);
}

void MainWindow::on_actionExport_Map_Timelapse_Image_triggered() {
    showExportMapImageWindow(ImageExporterMode::Timelapse);
}

void MainWindow::on_actionImport_Map_from_Advance_Map_1_92_triggered(){
    importMapFromAdvanceMap1_92();
}

void MainWindow::importMapFromAdvanceMap1_92()
{
    QString filepath = QFileDialog::getOpenFileName(
                this,
                QString("Import Map from Advance Map 1.92"),
                this->editor->project->importExportPath,
                "Advance Map 1.92 Map Files (*.map)");
    if (filepath.isEmpty()) {
        return;
    }
    this->editor->project->setImportExportPath(filepath);
    MapParser parser;
    bool error = false;
    MapLayout *mapLayout = parser.parse(filepath, &error, editor->project);
    if (error) {
        QMessageBox msgBox(this);
        msgBox.setText("Failed to import map from Advance Map 1.92 .map file.");
        QString message = QString("The .map file could not be processed. View porymap.log for specific errors.");
        msgBox.setInformativeText(message);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Icon::Critical);
        msgBox.exec();
        return;
    }

    openNewMapWindow();
    this->newMapDialog->init(mapLayout);
}

void MainWindow::showExportMapImageWindow(ImageExporterMode mode) {
    if (!editor->project) return;

    // If the user is requesting this window again we assume it's for a new
    // window (the map/mode may have changed), so delete the old window.
    if (this->mapImageExporter)
        delete this->mapImageExporter;

    this->mapImageExporter = new MapImageExporter(this, this->editor, mode);

    openSubWindow(this->mapImageExporter);
}

void MainWindow::on_pushButton_AddConnection_clicked() {
    if (!this->editor || !this->editor->map || !this->editor->project)
        return;

    auto dialog = new NewMapConnectionDialog(this, this->editor->map, this->editor->project->mapNames);
    connect(dialog, &NewMapConnectionDialog::accepted, this->editor, &Editor::addConnection);
    dialog->exec();
}

void MainWindow::on_pushButton_NewWildMonGroup_clicked() {
    editor->addNewWildMonGroup(this);
}

void MainWindow::on_pushButton_DeleteWildMonGroup_clicked() {
    editor->deleteWildMonGroup();
}

void MainWindow::on_pushButton_SummaryChart_clicked() {
    if (!this->wildMonChart) {
        this->wildMonChart = new WildMonChart(this, this->editor->getCurrentWildMonTable());
        connect(this->editor, &Editor::wildMonTableOpened, this->wildMonChart, &WildMonChart::setTable);
        connect(this->editor, &Editor::wildMonTableClosed, this->wildMonChart, &WildMonChart::clearTable);
        connect(this->editor, &Editor::wildMonTableEdited, this->wildMonChart, &WildMonChart::refresh);
    }
    openSubWindow(this->wildMonChart);
}

void MainWindow::on_pushButton_ConfigureEncountersJSON_clicked() {
    editor->configureEncounterJSON(this);
}

void MainWindow::on_button_OpenDiveMap_clicked() {
    userSetMap(ui->comboBox_DiveMap->currentText(), true);
}

void MainWindow::on_button_OpenEmergeMap_clicked() {
    userSetMap(ui->comboBox_EmergeMap->currentText(), true);
}

void MainWindow::on_comboBox_DiveMap_currentTextChanged(const QString &mapName) {
    // Include empty names as an update (user is deleting the connection)
    if (mapName.isEmpty() || editor->project->mapNames.contains(mapName))
        editor->updateDiveMap(mapName);
}

void MainWindow::on_comboBox_EmergeMap_currentTextChanged(const QString &mapName) {
    if (mapName.isEmpty() || editor->project->mapNames.contains(mapName))
        editor->updateEmergeMap(mapName);
}

void MainWindow::on_comboBox_PrimaryTileset_currentTextChanged(const QString &tilesetLabel)
{
    if (editor->project->primaryTilesetLabels.contains(tilesetLabel) && editor->map) {
        editor->updatePrimaryTileset(tilesetLabel);
        redrawMapScene();
        on_horizontalSlider_MetatileZoom_valueChanged(ui->horizontalSlider_MetatileZoom->value());
        updateTilesetEditor();
        prefab.updatePrefabUi(editor->map);
        markMapEdited();
    }
}

void MainWindow::on_comboBox_SecondaryTileset_currentTextChanged(const QString &tilesetLabel)
{
    if (editor->project->secondaryTilesetLabels.contains(tilesetLabel) && editor->map) {
        editor->updateSecondaryTileset(tilesetLabel);
        redrawMapScene();
        on_horizontalSlider_MetatileZoom_valueChanged(ui->horizontalSlider_MetatileZoom->value());
        updateTilesetEditor();
        prefab.updatePrefabUi(editor->map);
        markMapEdited();
    }
}

void MainWindow::on_pushButton_ChangeDimensions_clicked()
{
    QDialog dialog(this, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    dialog.setWindowTitle("Change Map Dimensions");
    dialog.setWindowModality(Qt::NonModal);

    QFormLayout form(&dialog);

    QSpinBox *widthSpinBox = new QSpinBox();
    QSpinBox *heightSpinBox = new QSpinBox();
    QSpinBox *bwidthSpinBox = new QSpinBox();
    QSpinBox *bheightSpinBox = new QSpinBox();
    widthSpinBox->setMinimum(1);
    heightSpinBox->setMinimum(1);
    bwidthSpinBox->setMinimum(1);
    bheightSpinBox->setMinimum(1);
    widthSpinBox->setMaximum(editor->project->getMaxMapWidth());
    heightSpinBox->setMaximum(editor->project->getMaxMapHeight());
    bwidthSpinBox->setMaximum(MAX_BORDER_WIDTH);
    bheightSpinBox->setMaximum(MAX_BORDER_HEIGHT);
    widthSpinBox->setValue(editor->map->getWidth());
    heightSpinBox->setValue(editor->map->getHeight());
    bwidthSpinBox->setValue(editor->map->getBorderWidth());
    bheightSpinBox->setValue(editor->map->getBorderHeight());
    if (projectConfig.useCustomBorderSize) {
        form.addRow(new QLabel("Map Width"), widthSpinBox);
        form.addRow(new QLabel("Map Height"), heightSpinBox);
        form.addRow(new QLabel("Border Width"), bwidthSpinBox);
        form.addRow(new QLabel("Border Height"), bheightSpinBox);
    } else {
        form.addRow(new QLabel("Width"), widthSpinBox);
        form.addRow(new QLabel("Height"), heightSpinBox);
    }

    QLabel *errorLabel = new QLabel();
    errorLabel->setStyleSheet("QLabel { color: red }");
    errorLabel->setVisible(false);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, [&dialog, &widthSpinBox, &heightSpinBox, &errorLabel, this](){
        // Ensure width and height are an acceptable size.
        // The maximum number of metatiles in a map is the following:
        //    max = (width + 15) * (height + 14)
        // This limit can be found in fieldmap.c in pokeruby/pokeemerald/pokefirered.
        int numMetatiles = editor->project->getMapDataSize(widthSpinBox->value(), heightSpinBox->value());
        int maxMetatiles = editor->project->getMaxMapDataSize();
        if (numMetatiles <= maxMetatiles) {
            dialog.accept();
        } else {
            QString errorText = QString("Error: The specified width and height are too large.\n"
                    "The maximum map width and height is the following: (width + 15) * (height + 14) <= %1\n"
                    "The specified map width and height was: (%2 + 15) * (%3 + 14) = %4")
                        .arg(maxMetatiles)
                        .arg(widthSpinBox->value())
                        .arg(heightSpinBox->value())
                        .arg(numMetatiles);
            errorLabel->setText(errorText);
            errorLabel->setVisible(true);
        }
    });
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    form.addRow(errorLabel);

    if (dialog.exec() == QDialog::Accepted) {
        Map *map = editor->map;
        Blockdata oldMetatiles = map->layout->blockdata;
        Blockdata oldBorder = map->layout->border;
        QSize oldMapDimensions(map->getWidth(), map->getHeight());
        QSize oldBorderDimensions(map->getBorderWidth(), map->getBorderHeight());
        QSize newMapDimensions(widthSpinBox->value(), heightSpinBox->value());
        QSize newBorderDimensions(bwidthSpinBox->value(), bheightSpinBox->value());
        if (oldMapDimensions != newMapDimensions || oldBorderDimensions != newBorderDimensions) {
            editor->map->setDimensions(newMapDimensions.width(), newMapDimensions.height(), true, true);
            editor->map->setBorderDimensions(newBorderDimensions.width(), newBorderDimensions.height(), true, true);
            editor->map->editHistory.push(new ResizeMap(map,
                oldMapDimensions, newMapDimensions,
                oldMetatiles, map->layout->blockdata,
                oldBorderDimensions, newBorderDimensions,
                oldBorder, map->layout->border
            ));
        }
    }
}

void MainWindow::on_checkBox_smartPaths_stateChanged(int selected)
{
    bool enabled = selected == Qt::Checked;
    editor->settings->smartPathsEnabled = enabled;
    if (enabled) {
        editor->cursorMapTileRect->setSmartPathMode(true);
    } else {
        editor->cursorMapTileRect->setSmartPathMode(false);
    }
}

void MainWindow::on_checkBox_ToggleBorder_stateChanged(int selected)
{
    editor->toggleBorderVisibility(selected != 0);
}

void MainWindow::on_checkBox_MirrorConnections_stateChanged(int selected)
{
    porymapConfig.mirrorConnectingMaps = (selected == Qt::Checked);
}

void MainWindow::on_actionTileset_Editor_triggered()
{
    if (!this->tilesetEditor) {
        initTilesetEditor();
    }

    openSubWindow(this->tilesetEditor);

    MetatileSelection selection = this->editor->metatile_selector_item->getMetatileSelection();
    this->tilesetEditor->selectMetatile(selection.metatileItems.first().metatileId);
}

void MainWindow::initTilesetEditor() {
    this->tilesetEditor = new TilesetEditor(this->editor->project, this->editor->map, this);
    connect(this->tilesetEditor, &TilesetEditor::tilesetsSaved, this, &MainWindow::onTilesetsSaved);
}

void MainWindow::on_toolButton_ExpandAll_clicked()
{
    if (ui->mapList) {
        ui->mapList->expandToDepth(0);
    }
}

void MainWindow::on_toolButton_CollapseAll_clicked()
{
    if (ui->mapList) {
        ui->mapList->collapseAll();
    }
}

void MainWindow::on_actionAbout_Porymap_triggered()
{
    if (!this->aboutWindow)
        this->aboutWindow = new AboutPorymap(this);
    openSubWindow(this->aboutWindow);
}

void MainWindow::on_actionOpen_Log_File_triggered() {
    const QString logPath = getLogPath();
    const int lineCount = ParseUtil::textFileLineCount(logPath);
    this->editor->openInTextEditor(logPath, lineCount);
}

void MainWindow::on_actionOpen_Config_Folder_triggered() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
}

void MainWindow::on_actionPreferences_triggered() {
    if (!preferenceEditor) {
        preferenceEditor = new PreferenceEditor(this);
        connect(preferenceEditor, &PreferenceEditor::themeChanged, this, &MainWindow::setTheme);
        connect(preferenceEditor, &PreferenceEditor::themeChanged, editor, &Editor::maskNonVisibleConnectionTiles);
        connect(preferenceEditor, &PreferenceEditor::preferencesSaved, this, &MainWindow::togglePreferenceSpecificUi);
    }

    openSubWindow(preferenceEditor);
}

void MainWindow::togglePreferenceSpecificUi() {
    if (porymapConfig.textEditorOpenFolder.isEmpty())
        ui->actionOpen_Project_in_Text_Editor->setEnabled(false);
    else
        ui->actionOpen_Project_in_Text_Editor->setEnabled(true);

    ui->newEventToolButton->setActionVisible(Event::Type::HealLocation, porymapConfig.allowHealLocationDeleting);

    if (this->updatePromoter)
        this->updatePromoter->updatePreferences();
}

void MainWindow::openProjectSettingsEditor(int tab) {
    if (!this->projectSettingsEditor) {
        this->projectSettingsEditor = new ProjectSettingsEditor(this, this->editor->project);
        connect(this->projectSettingsEditor, &ProjectSettingsEditor::reloadProject,
                this, &MainWindow::on_action_Reload_Project_triggered);
    }
    this->projectSettingsEditor->setTab(tab);
    openSubWindow(this->projectSettingsEditor);
}

void MainWindow::on_actionProject_Settings_triggered() {
    this->openProjectSettingsEditor(porymapConfig.projectSettingsTab);
}

void MainWindow::onWarpBehaviorWarningClicked() {
    static const QString text = QString("Warp Events only function as exits on certain metatiles");
    static const QString informative = QString(
        "<html><head/><body><p>"
        "For instance, most floor metatiles in a cave have the metatile behavior <b>MB_CAVE</b>, but the floor space in front of an exit "
        "will have <b>MB_SOUTH_ARROW_WARP</b>, which is treated specially in your project's code to allow a Warp Event to warp the player. "
        "<br><br>"
        "You can see in the status bar what behavior a metatile has when you mouse over it, or by selecting it in the Tileset Editor. "
        "The warning will disappear when the warp is positioned on a metatile with a behavior known to allow warps."
        "<br><br>"
        "<b>Note</b>: Not all Warp Events that show this warning are incorrect! For example some warps may function "
        "as a 1-way entrance, and others may have the metatile underneath them changed programmatically."
        "<br><br>"
        "You can disable this warning or edit the list of behaviors that silence this warning under <b>Options -> Project Settings...</b>"
        "<br></html></body></p>"
    );
    QMessageBox msgBox(QMessageBox::Information, "porymap", text, QMessageBox::Close, this);
    QPushButton *settings = msgBox.addButton("Open Settings...", QMessageBox::ActionRole);
    msgBox.setDefaultButton(QMessageBox::Close);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setInformativeText(informative);
    msgBox.exec();
    if (msgBox.clickedButton() == settings)
        this->openProjectSettingsEditor(ProjectSettingsEditor::eventsTab);
}

void MainWindow::on_actionCustom_Scripts_triggered() {
    if (!this->customScriptsEditor)
        initCustomScriptsEditor();

    openSubWindow(this->customScriptsEditor);
}

void MainWindow::initCustomScriptsEditor() {
    this->customScriptsEditor = new CustomScriptsEditor(this);
    connect(this->customScriptsEditor, &CustomScriptsEditor::reloadScriptEngine,
            this, &MainWindow::reloadScriptEngine);
}

void MainWindow::reloadScriptEngine() {
    Scripting::init(this);
    Scripting::populateGlobalObject(this);
    // Lying to the scripts here, simulating a project reload
    Scripting::cb_ProjectOpened(projectConfig.projectDir);
    if (editor && editor->map)
        Scripting::cb_MapOpened(editor->map->name);
}

void MainWindow::on_pushButton_AddCustomHeaderField_clicked()
{
    bool ok;
    QJsonValue value = CustomAttributesTable::pickType(this, &ok);
    if (ok){
        CustomAttributesTable::addAttribute(this->ui->tableWidget_CustomHeaderFields, "", value, true);
        this->editor->updateCustomMapHeaderValues(this->ui->tableWidget_CustomHeaderFields);
    }
}

void MainWindow::on_pushButton_DeleteCustomHeaderField_clicked()
{
    if (CustomAttributesTable::deleteSelectedAttributes(this->ui->tableWidget_CustomHeaderFields))
        this->editor->updateCustomMapHeaderValues(this->ui->tableWidget_CustomHeaderFields);
}

void MainWindow::on_tableWidget_CustomHeaderFields_cellChanged(int, int)
{
    this->editor->updateCustomMapHeaderValues(this->ui->tableWidget_CustomHeaderFields);
}

void MainWindow::on_horizontalSlider_MetatileZoom_valueChanged(int value) {
    porymapConfig.metatilesZoom = value;
    double scale = pow(3.0, static_cast<double>(value - 30) / 30.0);

    QTransform transform;
    transform.scale(scale, scale);
    QSize size(editor->metatile_selector_item->pixmap().width(), 
               editor->metatile_selector_item->pixmap().height());
    size *= scale;

    ui->graphicsView_Metatiles->setResizeAnchor(QGraphicsView::NoAnchor);
    ui->graphicsView_Metatiles->setTransform(transform);
    ui->graphicsView_Metatiles->setFixedSize(size.width() + 2, size.height() + 2);

    ui->graphicsView_BorderMetatile->setTransform(transform);
    ui->graphicsView_BorderMetatile->setFixedSize(ceil(static_cast<double>(editor->selected_border_metatiles_item->pixmap().width()) * scale) + 2,
                                                  ceil(static_cast<double>(editor->selected_border_metatiles_item->pixmap().height()) * scale) + 2);

    ui->scrollAreaWidgetContents_MetatileSelector->adjustSize();
    ui->scrollAreaWidgetContents_BorderMetatiles->adjustSize();

    redrawMetatileSelection();
    scrollMetatileSelectorToSelection();
}

void MainWindow::on_horizontalSlider_CollisionZoom_valueChanged(int value) {
    porymapConfig.collisionZoom = value;
    double scale = pow(3.0, static_cast<double>(value - 30) / 30.0);

    QTransform transform;
    transform.scale(scale, scale);
    QSize size(editor->movement_permissions_selector_item->pixmap().width(),
               editor->movement_permissions_selector_item->pixmap().height());
    size *= scale;

    ui->graphicsView_Collision->setResizeAnchor(QGraphicsView::NoAnchor);
    ui->graphicsView_Collision->setTransform(transform);
    ui->graphicsView_Collision->setFixedSize(size.width() + 2, size.height() + 2);
    ui->scrollAreaWidgetContents_Collision->adjustSize();
}

void MainWindow::on_spinBox_SelectedCollision_valueChanged(int collision) {
    if (this->editor && this->editor->movement_permissions_selector_item)
        this->editor->movement_permissions_selector_item->select(collision, ui->spinBox_SelectedElevation->value());
}

void MainWindow::on_spinBox_SelectedElevation_valueChanged(int elevation) {
    if (this->editor && this->editor->movement_permissions_selector_item)
        this->editor->movement_permissions_selector_item->select(ui->spinBox_SelectedCollision->value(), elevation);
}

void MainWindow::on_actionRegion_Map_Editor_triggered() {
    if (!this->regionMapEditor) {
        if (!initRegionMapEditor()) {
            return;
        }
    }

    openSubWindow(this->regionMapEditor);
}

void MainWindow::on_pushButton_CreatePrefab_clicked() {
    PrefabCreationDialog dialog(this, this->editor->metatile_selector_item, this->editor->map);
    dialog.setWindowTitle("Create Prefab");
    dialog.setWindowModality(Qt::NonModal);
    if (dialog.exec() == QDialog::Accepted) {
        dialog.savePrefab();
    }
}

bool MainWindow::initRegionMapEditor(bool silent) {
    this->regionMapEditor = new RegionMapEditor(this, this->editor->project);
    if (!this->regionMapEditor->load(silent)) {
        // The region map editor either failed to load,
        // or the user declined configuring their settings.
        if (!silent && this->regionMapEditor->setupErrored()) {
            if (this->askToFixRegionMapEditor())
                return true;
        }
        delete this->regionMapEditor;
        return false;
    }

    return true;
}

bool MainWindow::askToFixRegionMapEditor() {
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText(QString("There was an error opening the region map data. Please see %1 for full error details.").arg(getLogPath()));
    msgBox.setDetailedText(getMostRecentError());
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    auto reconfigButton = msgBox.addButton("Reconfigure", QMessageBox::ActionRole);
    msgBox.exec();
    if (msgBox.clickedButton() == reconfigButton) {
        if (this->regionMapEditor->reconfigure()) {
            // User fixed error
            return true;
        }
        if (this->regionMapEditor->setupErrored()) {
            // User's new settings still fail, show error and ask again
            return this->askToFixRegionMapEditor();
        }
    }
    // User accepted error
    return false;
}

// Attempt to close any open sub-windows of the main window, giving each a chance to abort the process.
// Each of these windows is a widget with WA_DeleteOnClose set, so manually deleting them isn't necessary.
// Because they're tracked with QPointers nullifying them shouldn't be necessary either, but it seems the
// delete is happening too late and some of the pointers haven't been cleared by the time we need them to,
// so we nullify them all here anyway.
bool MainWindow::closeSupplementaryWindows() {
    if (this->tilesetEditor && !this->tilesetEditor->close())
        return false;
    this->tilesetEditor = nullptr;

    if (this->regionMapEditor && !this->regionMapEditor->close())
        return false;
    this->regionMapEditor = nullptr;

    if (this->mapImageExporter && !this->mapImageExporter->close())
        return false;
    this->mapImageExporter = nullptr;

    if (this->newMapDialog && !this->newMapDialog->close())
        return false;
    this->newMapDialog = nullptr;

    if (this->shortcutsEditor && !this->shortcutsEditor->close())
        return false;
    this->shortcutsEditor = nullptr;

    if (this->preferenceEditor && !this->preferenceEditor->close())
        return false;
    this->preferenceEditor = nullptr;

    if (this->customScriptsEditor && !this->customScriptsEditor->close())
        return false;
    this->customScriptsEditor = nullptr;

    if (this->wildMonChart && !this->wildMonChart->close())
        return false;
    this->wildMonChart = nullptr;

    if (this->projectSettingsEditor) this->projectSettingsEditor->closeQuietly();
    this->projectSettingsEditor = nullptr;

    return true;
}

bool MainWindow::closeProject() {
    if (!closeSupplementaryWindows())
        return false;

    if (!isProjectOpen())
        return true;

    // Check loaded maps for unsaved changes
    bool unsavedChanges = false;
    for (auto map : editor->project->mapCache.values()) {
        if (map && map->hasUnsavedChanges()) {
            unsavedChanges = true;
            break;
        }
    }

    if (unsavedChanges) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this, "porymap", "The project has been modified, save changes?",
            QMessageBox::No | QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);

        if (result == QMessageBox::Yes) {
            editor->saveProject();
        } else if (result == QMessageBox::No) {
            logWarn("Closing project with unsaved changes.");
        } else if (result == QMessageBox::Cancel) {
            return false;
        }
    }
    clearProjectUI();
    editor->closeProject();
    setWindowDisabled(true);
    setWindowTitle(QCoreApplication::applicationName());

    return true;
}

void MainWindow::on_action_Exit_triggered() {
    if (!closeProject())
        return;
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!closeProject()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}
