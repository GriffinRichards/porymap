#include "preferenceeditor.h"
#include "ui_preferenceeditor.h"
#include "config.h"
#include "noscrollcombobox.h"

#include <QAbstractButton>
#include <QRegularExpression>
#include <QDirIterator>
#include <QFormLayout>
#include <QMessageBox>


PreferenceEditor::PreferenceEditor(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PreferenceEditor),
    themeSelector(nullptr)
{
    ui->setupUi(this);
    auto *formLayout = new QFormLayout(ui->groupBox_Themes);
    themeSelector = new NoScrollComboBox(ui->groupBox_Themes);
    themeSelector->setEditable(false);
    themeSelector->setMinimumContentsLength(0);
    formLayout->addRow("Themes", themeSelector);
    setAttribute(Qt::WA_DeleteOnClose);
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &PreferenceEditor::dialogButtonClicked);
    connect(ui->checkBox_HealLocationEvents, &QCheckBox::toggled, [this](bool on) { if (on) showHealLocationWarning(); });
    initFields();
    updateFields();
}

PreferenceEditor::~PreferenceEditor()
{
    delete ui;
}

void PreferenceEditor::initFields() {
    QStringList themes = { "default" };
    static const QRegularExpression re(":/themes/([A-z0-9_-]+).qss");
    QDirIterator it(":/themes", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString themeName = re.match(it.next()).captured(1);
        themes.append(themeName);
    }
    themeSelector->addItems(themes);
}

void PreferenceEditor::updateFields() {
    themeSelector->setCurrentText(porymapConfig.theme);
    ui->lineEdit_TextEditorOpenFolder->setText(porymapConfig.textEditorOpenFolder);
    ui->lineEdit_TextEditorGotoLine->setText(porymapConfig.textEditorGotoLine);
    ui->checkBox_MonitorProjectFiles->setChecked(porymapConfig.monitorFiles);
    ui->checkBox_OpenRecentProject->setChecked(porymapConfig.reopenOnLaunch);
    ui->checkBox_CheckForUpdates->setChecked(porymapConfig.checkForUpdates);
    if (porymapConfig.eventSelectionShapeMode == QGraphicsPixmapItem::MaskShape) {
        ui->radioButton_OnSprite->setChecked(true);
    } else if (porymapConfig.eventSelectionShapeMode == QGraphicsPixmapItem::BoundingRectShape) {
        ui->radioButton_WithinRect->setChecked(true);
    }

    const QSignalBlocker b_Heal(ui->checkBox_HealLocationEvents);
    ui->checkBox_HealLocationEvents->setChecked(porymapConfig.allowHealLocationDeleting);
}

void PreferenceEditor::saveFields() {
    if (themeSelector->currentText() != porymapConfig.theme) {
        const auto theme = themeSelector->currentText();
        porymapConfig.theme = theme;
        emit themeChanged(theme);
    }

    porymapConfig.textEditorOpenFolder = ui->lineEdit_TextEditorOpenFolder->text();
    porymapConfig.textEditorGotoLine = ui->lineEdit_TextEditorGotoLine->text();
    porymapConfig.monitorFiles = ui->checkBox_MonitorProjectFiles->isChecked();
    porymapConfig.reopenOnLaunch = ui->checkBox_OpenRecentProject->isChecked();
    porymapConfig.checkForUpdates = ui->checkBox_CheckForUpdates->isChecked();
    porymapConfig.allowHealLocationDeleting = ui->checkBox_HealLocationEvents->isChecked();
    porymapConfig.eventSelectionShapeMode = ui->radioButton_OnSprite->isChecked() ? QGraphicsPixmapItem::MaskShape : QGraphicsPixmapItem::BoundingRectShape;
    porymapConfig.save();

    emit preferencesSaved();
}

void PreferenceEditor::showHealLocationWarning() {
    static const QString warning = "Deleting Heal Locations will delete their associated ID. This may stop your project from compiling. Are you sure you want to enable this setting?";
    if (QMessageBox::warning(this, "WARNING", warning, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        // User didn't accept warning, restore setting
        const QSignalBlocker b(ui->checkBox_HealLocationEvents);
        ui->checkBox_HealLocationEvents->setChecked(false);
    }
}

void PreferenceEditor::dialogButtonClicked(QAbstractButton *button) {
    auto buttonRole = ui->buttonBox->buttonRole(button);
    if (buttonRole == QDialogButtonBox::AcceptRole) {
        saveFields();
        close();
    } else if (buttonRole == QDialogButtonBox::ApplyRole) {
        saveFields();
    } else if (buttonRole == QDialogButtonBox::RejectRole) {
        close();
    }
}
