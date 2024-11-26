#include "newlayoutdialog.h"
#include "maplayout.h"
#include "ui_newlayoutdialog.h"
#include "config.h"

#include <QMap>
#include <QSet>
#include <QStringList>

const QString lineEdit_ErrorStylesheet = "QLineEdit { background-color: rgba(255, 0, 0, 25%) }";

NewLayoutDialog::NewLayoutDialog(Project *project, QWidget *parent) :
    NewLayoutDialog(project, nullptr, parent)
{}

NewLayoutDialog::NewLayoutDialog(Project *project, const Layout *layoutToCopy, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewLayoutDialog),
    layoutToCopy(layoutToCopy)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);
    ui->setupUi(this);
    this->project = project;

    QString newName;
    QString newId;
    if (this->layoutToCopy && !this->layoutToCopy->name.isEmpty()) {
        // Duplicating a layout, the initial name will be the base layout's name
        // with a numbered suffix to make it unique.
        // Note: Layouts imported with AdvanceMap have no name, so they'll use the default new layout name instead.

        // If the layout name ends with the default '_Layout' suffix we'll ignore it.
        // This is because (normally) the ID for these layouts will not have this suffix,
        // so you can end up in a situation where you might have Map_Layout and Map_2_Layout,
        // and if you try to duplicate Map_Layout the next available name (because of ID collisions)
        // would be Map_Layout_3 instead of Map_3_Layout.
        QString baseName = this->layoutToCopy->name;
        QString suffix = "_Layout";
        if (baseName.length() > suffix.length() && baseName.endsWith(suffix)) {
            baseName.truncate(baseName.length() - suffix.length());
        } else {
            suffix = "";
        }

        int i = 2;
        do {
            newName = QString("%1_%2%3").arg(baseName).arg(i).arg(suffix);
            newId = QString("%1_%2").arg(this->layoutToCopy->id).arg(i);
            i++;
        } while (!project->isIdentifierUnique(newName) || !project->isIdentifierUnique(newId));
    } else {
        newName = project->getNewLayoutName();
        newId = Layout::layoutConstantFromName(newName);
    }

    // We reset these settings for every session with the new layout dialog.
    // The rest of the settings are preserved in the project between sessions.
    project->newLayoutSettings.name = newName;
    project->newLayoutSettings.id = newId;

    ui->newLayoutForm->initUi(project);

    // Identifiers can only contain word characters, and cannot start with a digit.
    static const QRegularExpression re("[A-Za-z_]+[\\w]*");
    auto validator = new QRegularExpressionValidator(re, this);
    ui->lineEdit_Name->setValidator(validator);
    ui->lineEdit_LayoutID->setValidator(validator);

    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &NewLayoutDialog::dialogButtonClicked);

    refresh();
    adjustSize();
}

NewLayoutDialog::~NewLayoutDialog()
{
    saveSettings();
    delete ui;
}

void NewLayoutDialog::refresh() {
    const Layout::Settings *settings = &this->project->newLayoutSettings;

    if (this->layoutToCopy) {
        // If we're importing a layout then some settings will be enforced.
        ui->newLayoutForm->setSettings(this->layoutToCopy->settings());
        ui->newLayoutForm->setDisabled(true);
    } else {
        ui->newLayoutForm->setSettings(*settings);
        ui->newLayoutForm->setDisabled(false);
    }

    ui->lineEdit_Name->setText(settings->name);
    ui->lineEdit_LayoutID->setText(settings->id);
}

void NewLayoutDialog::saveSettings() {
    Layout::Settings *settings = &this->project->newLayoutSettings;

    *settings = ui->newLayoutForm->settings();
    settings->id = ui->lineEdit_LayoutID->text();
    settings->name = ui->lineEdit_Name->text();
}

bool NewLayoutDialog::validateLayoutID(bool allowEmpty) {
    QString id = ui->lineEdit_LayoutID->text();

    QString errorText;
    if (id.isEmpty()) {
        if (!allowEmpty) errorText = QString("%1 cannot be empty.").arg(ui->label_LayoutID->text());
    } else if (!this->project->isIdentifierUnique(id)) {
        errorText = QString("%1 '%2' is not unique.").arg(ui->label_LayoutID->text()).arg(id);
    }

    bool isValid = errorText.isEmpty();
    ui->label_LayoutIDError->setText(errorText);
    ui->label_LayoutIDError->setVisible(!isValid);
    ui->lineEdit_LayoutID->setStyleSheet(!isValid ? lineEdit_ErrorStylesheet : "");
    return isValid;
}

void NewLayoutDialog::on_lineEdit_LayoutID_textChanged(const QString &) {
    validateLayoutID(true);
}

bool NewLayoutDialog::validateName(bool allowEmpty) {
    QString name = ui->lineEdit_Name->text();

    QString errorText;
    if (name.isEmpty()) {
        if (!allowEmpty) errorText = QString("%1 cannot be empty.").arg(ui->label_Name->text());
    } else if (!this->project->isIdentifierUnique(name)) {
        errorText = QString("%1 '%2' is not unique.").arg(ui->label_Name->text()).arg(name);
    }

    bool isValid = errorText.isEmpty();
    ui->label_NameError->setText(errorText);
    ui->label_NameError->setVisible(!isValid);
    ui->lineEdit_Name->setStyleSheet(!isValid ? lineEdit_ErrorStylesheet : "");
    return isValid;
}

void NewLayoutDialog::on_lineEdit_Name_textChanged(const QString &text) {
    validateName(true);
    
    // Changing the layout name updates the layout ID field to match.
    ui->lineEdit_LayoutID->setText(Layout::layoutConstantFromName(text));
}

void NewLayoutDialog::dialogButtonClicked(QAbstractButton *button) {
    auto role = ui->buttonBox->buttonRole(button);
    if (role == QDialogButtonBox::RejectRole){
        reject();
    } else if (role == QDialogButtonBox::ResetRole) {
        this->project->initNewLayoutSettings();
        refresh();
    } else if (role == QDialogButtonBox::AcceptRole) {
        accept();
    }
}

void NewLayoutDialog::accept() {
    // Make sure to call each validation function so that all errors are shown at once.
    bool success = true;
    if (!ui->newLayoutForm->validate()) success = false;
    if (!validateLayoutID()) success = false;
    if (!validateName()) success = false;
    if (!success)
        return;

    // Update settings from UI
    saveSettings();

    Layout *layout = this->project->createNewLayout(this->project->newLayoutSettings, this->layoutToCopy);
    if (!layout) {
        ui->label_GenericError->setText(QString("Failed to create layout. See %1 for details.").arg(getLogPath()));
        ui->label_GenericError->setVisible(true);
        return;
    }
    ui->label_GenericError->setVisible(false);

    // TODO: See if we can get away with emitting this from Project so that we don't need to connect
    //       to this signal every time we create the dialog.
    emit applied(layout->id);
    QDialog::accept();
}
