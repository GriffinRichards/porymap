#ifndef NEWMAPDIALOG_H
#define NEWMAPDIALOG_H

#include <QDialog>
#include <QString>
#include "editor.h"
#include "project.h"
#include "map.h"
#include "mapheaderform.h"
#include "newlayoutform.h"
#include "lib/collapsiblesection.h"

namespace Ui {
class NewMapDialog;
}

class NewMapDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewMapDialog(Project *project, QWidget *parent = nullptr);
    explicit NewMapDialog(Project *project, const Map *mapToCopy = nullptr, QWidget *parent = nullptr);
    explicit NewMapDialog(Project *project, int mapListTab, const QString &mapListItem, QWidget *parent = nullptr);
    ~NewMapDialog();

    virtual void accept() override;

private:
    Ui::NewMapDialog *ui;
    Project *project;
    CollapsibleSection *headerSection;
    MapHeaderForm *headerForm;
    const Map *mapToCopy;

    // Each of these validation functions will allow empty names up until `OK` is selected,
    // because clearing the text during editing is common and we don't want to flash errors for this.
    bool validateName(bool allowEmpty = false);
    bool validateGroup(bool allowEmpty = false);
    bool validateLayoutID(bool allowEmpty = false);

    void refresh();
    void saveSettings();

private slots:
    void dialogButtonClicked(QAbstractButton *button);
    void on_lineEdit_Name_textChanged(const QString &);
    void on_comboBox_Group_currentTextChanged(const QString &text);
    void on_comboBox_LayoutID_currentTextChanged(const QString &text);
};

#endif // NEWMAPDIALOG_H
