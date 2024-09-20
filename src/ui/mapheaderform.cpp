#include "mapheaderform.h"

#define BLOCK_SIGNALS \
    const QSignalBlocker b_Song(ui->comboBox_Song); \
    const QSignalBlocker b_Location(ui->comboBox_Location); \
    const QSignalBlocker b_RequiresFlash(ui->checkBox_RequiresFlash); \
    const QSignalBlocker b_Weather(ui->comboBox_Weather); \
    const QSignalBlocker b_Type(ui->comboBox_Type); \
    const QSignalBlocker b_BattleScene(ui->comboBox_BattleScene); \
    const QSignalBlocker b_ShowLocationName(ui->checkBox_ShowLocationName); \
    const QSignalBlocker b_AllowRunning(ui->checkBox_AllowRunning); \
    const QSignalBlocker b_AllowBiking(ui->checkBox_AllowBiking); \
    const QSignalBlocker b_AllowEscaping(ui->checkBox_AllowEscaping); \
    const QSignalBlocker b_FloorNumber(ui->spinBox_FloorNumber);


MapHeaderForm::MapHeaderForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MapHeaderForm)
{
    ui->setupUi(this);

    // This value is an s8 by default, but we don't need to unnecessarily limit users.
    ui->spinBox_FloorNumber->setMinimum(INT_MIN);
    ui->spinBox_FloorNumber->setMaximum(INT_MAX);
}

MapHeaderForm::~MapHeaderForm()
{
    delete ui;
}

void MapHeaderForm::setProject(Project * newProject) {
    clear();

    this->project = newProject;
    if (!this->project)
        return;

    // Populate combo boxes
    BLOCK_SIGNALS
    ui->comboBox_Song->addItems(this->project->songNames);
    ui->comboBox_Location->addItems(this->project->mapSectionNameToValue.keys());
    ui->comboBox_Weather->addItems(this->project->weatherNames);
    ui->comboBox_Type->addItems(this->project->mapTypes);
    ui->comboBox_BattleScene->addItems(this->project->mapBattleScenes);

    // Hide config-specific settings

    bool hasFlags = projectConfig.mapAllowFlagsEnabled;
    ui->checkBox_AllowRunning->setVisible(hasFlags);
    ui->checkBox_AllowBiking->setVisible(hasFlags);
    ui->checkBox_AllowEscaping->setVisible(hasFlags);
    ui->label_AllowRunning->setVisible(hasFlags);
    ui->label_AllowBiking->setVisible(hasFlags);
    ui->label_AllowEscaping->setVisible(hasFlags);

    bool floorNumEnabled = projectConfig.floorNumberEnabled;
    ui->spinBox_FloorNumber->setVisible(floorNumEnabled);
    ui->label_FloorNumber->setVisible(floorNumEnabled);
}

void MapHeaderForm::setMap(Map * newMap) {
    this->map = newMap;
    if (!this->map) {
        clearDisplay();
        return;
    }

    BLOCK_SIGNALS
    ui->comboBox_Song->setCurrentText(this->map->song);
    ui->comboBox_Location->setCurrentText(this->map->location);
    ui->checkBox_RequiresFlash->setChecked(this->map->requiresFlash);
    ui->comboBox_Weather->setCurrentText(this->map->weather);
    ui->comboBox_Type->setCurrentText(this->map->type);
    ui->comboBox_BattleScene->setCurrentText(this->map->battle_scene);
    ui->checkBox_ShowLocationName->setChecked(this->map->show_location);
    ui->checkBox_AllowRunning->setChecked(this->map->allowRunning);
    ui->checkBox_AllowBiking->setChecked(this->map->allowBiking);
    ui->checkBox_AllowEscaping->setChecked(this->map->allowEscaping);
    ui->spinBox_FloorNumber->setValue(this->map->floorNumber);
}

void MapHeaderForm::clearDisplay() {
    BLOCK_SIGNALS
    ui->comboBox_Song->clearEditText();
    ui->comboBox_Location->clearEditText();
    ui->comboBox_Weather->clearEditText();
    ui->comboBox_Type->clearEditText();
    ui->comboBox_BattleScene->clearEditText();
    ui->checkBox_ShowLocationName->setChecked(false);
    ui->checkBox_RequiresFlash->setChecked(false);
    ui->checkBox_AllowRunning->setChecked(false);
    ui->checkBox_AllowBiking->setChecked(false);
    ui->checkBox_AllowEscaping->setChecked(false);
    ui->spinBox_FloorNumber->setValue(0);
}

// Clear display and depopulate combo boxes
void MapHeaderForm::clear() {
    BLOCK_SIGNALS
    ui->comboBox_Song->clear();
    ui->comboBox_Location->clear();
    ui->comboBox_Weather->clear();
    ui->comboBox_Type->clear();
    ui->comboBox_BattleScene->clear();
    ui->checkBox_ShowLocationName->setChecked(false);
    ui->checkBox_RequiresFlash->setChecked(false);
    ui->checkBox_AllowRunning->setChecked(false);
    ui->checkBox_AllowBiking->setChecked(false);
    ui->checkBox_AllowEscaping->setChecked(false);
    ui->spinBox_FloorNumber->setValue(0);
}

void MapHeaderForm::on_comboBox_Song_currentTextChanged(const QString &song)
{
    if (this->map) {
        this->map->song = song;
        this->map->modify();
    }
}

void MapHeaderForm::on_comboBox_Location_currentTextChanged(const QString &location)
{
    if (this->map) {
        this->map->location = location;
        this->map->modify();

        // Update cached location name in the project
        if (this->project)
            this->project->mapNameToMapSectionName.insert(this->map->name, this->map->location);
    }
}

void MapHeaderForm::on_comboBox_Weather_currentTextChanged(const QString &weather)
{
    if (this->map) {
        this->map->weather = weather;
        this->map->modify();
    }
}

void MapHeaderForm::on_comboBox_Type_currentTextChanged(const QString &type)
{
    if (this->map) {
        this->map->type = type;
        this->map->modify();
    }
}

void MapHeaderForm::on_comboBox_BattleScene_currentTextChanged(const QString &battle_scene)
{
    if (this->map) {
        this->map->battle_scene = battle_scene;
        this->map->modify();
    }
}

void MapHeaderForm::on_checkBox_RequiresFlash_stateChanged(int selected)
{
    if (this->map) {
        this->map->requiresFlash = (selected == Qt::Checked);
        this->map->modify();
    }
}

void MapHeaderForm::on_checkBox_ShowLocationName_stateChanged(int selected)
{
    if (this->map) {
        this->map->show_location = (selected == Qt::Checked);
        this->map->modify();
    }
}

void MapHeaderForm::on_checkBox_AllowRunning_stateChanged(int selected)
{
    if (this->map) {
        this->map->allowRunning = (selected == Qt::Checked);
        this->map->modify();
    }
}

void MapHeaderForm::on_checkBox_AllowBiking_stateChanged(int selected)
{
    if (this->map) {
        this->map->allowBiking = (selected == Qt::Checked);
        this->map->modify();
    }
}

void MapHeaderForm::on_checkBox_AllowEscaping_stateChanged(int selected)
{
    if (this->map) {
        this->map->allowEscaping = (selected == Qt::Checked);
        this->map->modify();
    }
}

void MapHeaderForm::on_spinBox_FloorNumber_valueChanged(int offset)
{
    if (this->map) {
        this->map->floorNumber = offset;
        this->map->modify();
    }
}
