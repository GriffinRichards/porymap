#include "editor.h"
#include "draggablepixmapitem.h"
#include "imageproviders.h"
#include "log.h"
#include "connectionslistitem.h"
#include "currentselectedmetatilespixmapitem.h"
#include "mapsceneeventfilter.h"
#include "metatile.h"
#include "montabwidget.h"
#include "editcommands.h"
#include "config.h"
#include "scripting.h"
#include "customattributestable.h"
#include <QCheckBox>
#include <QPainter>
#include <QMouseEvent>
#include <QDir>
#include <QProcess>
#include <math.h>

static bool selectNewEvents = false;

// 2D array mapping collision+elevation combos to an icon.
QList<QList<const QImage*>> Editor::collisionIcons;

Editor::Editor(Ui::MainWindow* ui)
{
    this->ui = ui;
    this->selected_events = new QList<DraggablePixmapItem*>;
    this->settings = new Settings();
    this->playerViewRect = new MovableRect(&this->settings->playerViewRectEnabled, 30 * 8, 20 * 8, qRgb(255, 255, 255));
    this->cursorMapTileRect = new CursorTileRect(&this->settings->cursorTileRectEnabled, qRgb(255, 255, 255));
    this->map_ruler = new MapRuler(4);
    connect(this->map_ruler, &MapRuler::statusChanged, this, &Editor::mapRulerStatusChanged);

    /// Instead of updating the selected events after every single undo action
    /// (eg when the user rolls back several at once), only reselect events when
    /// the index is changed.
    connect(&editGroup, &QUndoGroup::indexChanged, [this](int) {
        if (selectNewEvents) {
            updateSelectedEvents();
            selectNewEvents = false;
        }
    });

    // Send signals used for updating the wild pokemon summary chart
    connect(ui->stackedWidget_WildMons, &QStackedWidget::currentChanged, [this] {
        emit wildMonTableOpened(getCurrentWildMonTable());
    });
}

Editor::~Editor()
{
    delete this->selected_events;
    delete this->settings;
    delete this->playerViewRect;
    delete this->cursorMapTileRect;
    delete this->map_ruler;
    for (auto sublist : collisionIcons)
        qDeleteAll(sublist);

    closeProject();
}

void Editor::saveProject() {
    if (project) {
        saveUiFields();
        project->saveAllMaps();
        project->saveAllDataStructures();
    }
}

void Editor::save() {
    if (project && map) {
        saveUiFields();
        project->saveMap(map);
        project->saveAllDataStructures();
    }
}

void Editor::saveUiFields() {
    saveEncounterTabData();
}

void Editor::setProject(Project * project) {
    closeProject();
    this->project = project;
    MapConnection::project = project;
}

void Editor::closeProject() {
    if (!this->project)
        return;
    this->project->saveConfig();
    Scripting::cb_ProjectClosed(this->project->root);
    Scripting::stop();
    clearMap();
    delete this->project;
}

void Editor::setEditingMap() {
    current_view = map_item;
    if (map_item) {
        map_item->paintingMode = MapPixmapItem::PaintMode::Metatiles;
        map_item->draw();
        map_item->setVisible(true);
    }
    if (collision_item) {
        collision_item->setVisible(false);
    }
    if (events_group) {
        events_group->setVisible(false);
    }
    updateBorderVisibility();
    this->cursorMapTileRect->stopSingleTileMode();
    this->cursorMapTileRect->setActive(true);

    setMapEditingButtonsEnabled(true);
}

void Editor::setEditingCollision() {
    current_view = collision_item;
    if (collision_item) {
        collision_item->draw();
        collision_item->setVisible(true);
    }
    if (map_item) {
        map_item->paintingMode = MapPixmapItem::PaintMode::Metatiles;
        map_item->draw();
        map_item->setVisible(true);
    }
    if (events_group) {
        events_group->setVisible(false);
    }
    updateBorderVisibility();
    this->cursorMapTileRect->setSingleTileMode();
    this->cursorMapTileRect->setActive(true);

    setMapEditingButtonsEnabled(true);
}

void Editor::setEditingEvents() {
    current_view = map_item;
    if (events_group) {
        events_group->setVisible(true);
    }
    if (map_item) {
        map_item->paintingMode = MapPixmapItem::PaintMode::Events;
        map_item->draw();
        map_item->setVisible(true);
    }
    if (collision_item) {
        collision_item->setVisible(false);
    }
    updateBorderVisibility();
    this->cursorMapTileRect->setSingleTileMode();
    this->cursorMapTileRect->setActive(false);
    updateWarpEventWarnings();

    setMapEditingButtonsEnabled(false);
}

void Editor::setMapEditingButtonsEnabled(bool enabled) {
    this->ui->toolButton_Fill->setEnabled(enabled);
    this->ui->toolButton_Dropper->setEnabled(enabled);
    this->ui->pushButton_ChangeDimensions->setEnabled(enabled);
    // If the fill button is pressed, unpress it and select the pointer.
    if (!enabled && (this->ui->toolButton_Fill->isChecked() || this->ui->toolButton_Dropper->isChecked())) {
        this->map_edit_mode = "select";
        this->settings->mapCursor = QCursor();
        this->cursorMapTileRect->setSingleTileMode();
        this->ui->toolButton_Fill->setChecked(false);
        this->ui->toolButton_Dropper->setChecked(false);
        this->ui->toolButton_Select->setChecked(true);
    }
    this->ui->checkBox_smartPaths->setEnabled(enabled);
}

void Editor::setEditingConnections() {
    current_view = map_item;
    if (map_item) {
        map_item->paintingMode = MapPixmapItem::PaintMode::Disabled;
        map_item->draw();
        map_item->setVisible(true);
    }
    if (collision_item) {
        collision_item->setVisible(false);
    }
    if (events_group) {
        events_group->setVisible(false);
    }
    updateBorderVisibility();
    this->cursorMapTileRect->setSingleTileMode();
    this->cursorMapTileRect->setActive(false);
}

void Editor::clearWildMonTables() {
    QStackedWidget *stack = ui->stackedWidget_WildMons;
    const QSignalBlocker blocker(stack);

    // delete widgets from previous map data if they exist
    while (stack->count()) {
        QWidget *oldWidget = stack->widget(0);
        stack->removeWidget(oldWidget);
        delete oldWidget;
    }

    ui->comboBox_EncounterGroupLabel->clear();
    emit wildMonTableClosed();
}

void Editor::displayWildMonTables() {
    clearWildMonTables();

    // Don't try to read encounter data if it doesn't exist on disk for this map.
    if (!project->wildMonData.contains(map->constantName)) {
        return;
    }

    QComboBox *labelCombo = ui->comboBox_EncounterGroupLabel;
    for (auto groupPair : project->wildMonData[map->constantName])
        labelCombo->addItem(groupPair.first);

    labelCombo->setCurrentText(labelCombo->itemText(0));

    QStackedWidget *stack = ui->stackedWidget_WildMons;
    int labelIndex = 0;
    for (auto labelPair : project->wildMonData[map->constantName]) {

        QString label = labelPair.first;

        WildPokemonHeader header = project->wildMonData[map->constantName][label];

        MonTabWidget *tabWidget = new MonTabWidget(this);
        stack->insertWidget(labelIndex++, tabWidget);

        int tabIndex = 0;
        for (EncounterField monField : project->wildMonFields) {
            QString fieldName = monField.name;

            tabWidget->clearTableAt(tabIndex);

            if (project->wildMonData.contains(map->constantName) && header.wildMons[fieldName].active) {
                tabWidget->populateTab(tabIndex, header.wildMons[fieldName]);
            } else {
                tabWidget->setTabActive(tabIndex, false);
            }
            tabIndex++;
        }
        connect(tabWidget, &MonTabWidget::currentChanged, [this] {
            emit wildMonTableOpened(getCurrentWildMonTable());
        });
    }
    stack->setCurrentIndex(0);
    emit wildMonTableOpened(getCurrentWildMonTable());
}

void Editor::addNewWildMonGroup(QWidget *window) {
    QStackedWidget *stack = ui->stackedWidget_WildMons;
    QComboBox *labelCombo = ui->comboBox_EncounterGroupLabel;

    int stackIndex = stack->currentIndex();

    QDialog dialog(window, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    dialog.setWindowTitle("New Wild Encounter Group Label");
    dialog.setWindowModality(Qt::NonModal);

    QFormLayout form(&dialog);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);

    QLineEdit *lineEdit = new QLineEdit();
    lineEdit->setClearButtonEnabled(true);
    form.addRow(new QLabel("Group Base Label:"), lineEdit);
    static const QRegularExpression re_validChars("[_A-Za-z0-9]*");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(re_validChars);
    lineEdit->setValidator(validator);
    connect(lineEdit, &QLineEdit::textChanged, [this, &lineEdit, &buttonBox](QString text){
        if (this->project->encounterGroupLabels.contains(text)) {
            lineEdit->setStyleSheet("QLineEdit { background-color: rgba(255, 0, 0, 25%) }");
            buttonBox.button(QDialogButtonBox::Ok)->setDisabled(true);
        } else {
            lineEdit->setStyleSheet("");
            buttonBox.button(QDialogButtonBox::Ok)->setEnabled(true);
        }
    });
    // Give a default value to the label.
    lineEdit->setText(QString("g%1%2").arg(map->name).arg(stack->count()));

    // Fields [x] copy from existing
    QLabel *fieldsLabel = new QLabel("Fields:");
    form.addRow(fieldsLabel);
    QCheckBox *copyCheckbox = new QCheckBox;
    copyCheckbox->setEnabled(stack->count());
    form.addRow(new QLabel("Copy from current group"), copyCheckbox);
    QVector<QCheckBox *> fieldCheckboxes;
    for (EncounterField monField : project->wildMonFields) {
        QCheckBox *fieldCheckbox = new QCheckBox;
        fieldCheckboxes.append(fieldCheckbox);
        form.addRow(new QLabel(monField.name), fieldCheckbox);
    }
    // Reading from ui here so not saving to disk before user.
    connect(copyCheckbox, &QCheckBox::stateChanged, [=](int state){
        if (state == Qt::Checked) {
            int fieldIndex = 0;
            MonTabWidget *monWidget = static_cast<MonTabWidget *>(stack->widget(stack->currentIndex()));
            for (EncounterField monField : project->wildMonFields) {
                fieldCheckboxes[fieldIndex]->setChecked(monWidget->isTabEnabled(fieldIndex));
                fieldCheckboxes[fieldIndex]->setEnabled(false);
                fieldIndex++;
            }
        } else if (state == Qt::Unchecked) {
            int fieldIndex = 0;
            for (EncounterField monField : project->wildMonFields) {
                fieldCheckboxes[fieldIndex]->setEnabled(true);
                fieldIndex++;
            }
        }
    });

    connect(&buttonBox, &QDialogButtonBox::accepted, [&dialog, &lineEdit, this](){
        QString newLabel = lineEdit->text();
        if (!newLabel.isEmpty()) {
            this->project->encounterGroupLabels.append(newLabel);
            dialog.accept();
        }
    });
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form.addRow(&buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        WildPokemonHeader header;
        for (EncounterField& monField : project->wildMonFields) {
            QString fieldName = monField.name;
            header.wildMons[fieldName].active = false;
            header.wildMons[fieldName].encounterRate = 0;
        }

        MonTabWidget *tabWidget = new MonTabWidget(this);
        stack->insertWidget(stack->count(), tabWidget);

        labelCombo->addItem(lineEdit->text());
        labelCombo->setCurrentIndex(labelCombo->count() - 1);

        int tabIndex = 0;
        for (EncounterField &monField : project->wildMonFields) {
            QString fieldName = monField.name;
            tabWidget->clearTableAt(tabIndex);
            if (fieldCheckboxes[tabIndex]->isChecked()) {
                if (copyCheckbox->isChecked()) {
                    MonTabWidget *copyFrom = static_cast<MonTabWidget *>(stack->widget(stackIndex));
                    if (copyFrom->isTabEnabled(tabIndex)) {
                        QTableView *monTable = copyFrom->tableAt(tabIndex);
                        EncounterTableModel *model = static_cast<EncounterTableModel *>(monTable->model());
                        header.wildMons[fieldName] = model->encounterData();
                    }
                    else {
                        header.wildMons[fieldName] = getDefaultMonInfo(monField);
                    }
                } else {
                    header.wildMons[fieldName] = getDefaultMonInfo(monField);
                }
                tabWidget->populateTab(tabIndex, header.wildMons[fieldName]);
            } else {
                tabWidget->setTabActive(tabIndex, false);
            }
            tabIndex++;
        }
        saveEncounterTabData();
        emit wildMonTableEdited();
    }
}

void Editor::deleteWildMonGroup() {
    QComboBox *labelCombo = ui->comboBox_EncounterGroupLabel;

    if (labelCombo->count() < 1) {
        return;
    }

    QMessageBox msgBox;
    msgBox.setText("Confirm Delete");
    msgBox.setInformativeText("Are you sure you want to delete " + labelCombo->currentText() + "?");

    QPushButton *deleteButton = msgBox.addButton("Delete", QMessageBox::DestructiveRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.exec();

    if (msgBox.clickedButton() == deleteButton) {
        auto it = project->wildMonData.find(map->constantName);
        if (it == project->wildMonData.end()) {
          logError(QString("Failed to find data for map %1. Unable to delete").arg(map->constantName));
          return;
        }

        int i = project->encounterGroupLabels.indexOf(labelCombo->currentText());
        if (i < 0) {
          logError(QString("Failed to find selected wild mon group: %1. Unable to delete")
                   .arg(labelCombo->currentText()));
          return;
        }

        it.value().erase(labelCombo->currentText());
        project->encounterGroupLabels.remove(i);

        displayWildMonTables();
        saveEncounterTabData();
        emit wildMonTableEdited();
    }
}

void Editor::configureEncounterJSON(QWidget *window) {
    QVector<QWidget *> fieldSlots;

    EncounterFields tempFields = project->wildMonFields;

    QLabel *totalLabel = new QLabel;

    // lambda: Update the total displayed at the bottom of the Configure JSON
    //         window. Take groups into account when applicable.
    auto updateTotal = [&fieldSlots, totalLabel](EncounterField &currentField) {
        int total = 0, spinnerIndex = 0;
        QString groupTotalMessage;
        QMap<QString, int> groupTotals;
        for (auto keyPair : currentField.groups) {
            groupTotals.insert(keyPair.first, 0);// add to group map and initialize total to zero
        }
        for (auto slot : fieldSlots) {
            QSpinBox *spinner = slot->findChild<QSpinBox *>();
            int val = spinner->value();
            currentField.encounterRates[spinnerIndex] = val;
            if (!currentField.groups.empty()) {
                for (auto keyPair : currentField.groups) {
                    QString key = keyPair.first;
                    if (currentField.groups[key].contains(spinnerIndex)) {
                        groupTotals[key] += val;
                        break;
                    }
                }
            } else {
                total += val;
            }
            spinnerIndex++;
        }
        if (!currentField.groups.empty()) {
            groupTotalMessage += "Totals: ";
            for (auto keyPair : currentField.groups) {
                QString key = keyPair.first;
                groupTotalMessage += QString("%1 (%2),\t").arg(groupTotals[key]).arg(key);
            }
            groupTotalMessage.chop(2);
        } else {
            groupTotalMessage = QString("Total: %1").arg(QString::number(total));
        }
        if (total > 0xFF) {
            totalLabel->setTextFormat(Qt::RichText);
            groupTotalMessage += QString("<font color=\"red\">\tWARNING: value exceeds the limit for a u8 variable.</font>");
        }
        totalLabel->setText(groupTotalMessage);
    };

    // lambda: Create a new "slot", which is the widget containing a spinner and an index label. 
    //         Add the slot to a list of fieldSlots, which exists to keep track of them for memory management.
    auto createNewSlot = [&fieldSlots, &tempFields, &updateTotal](int index, EncounterField &currentField) {
        QLabel *indexLabel = new QLabel(QString("Index: %1").arg(QString::number(index)));
        QSpinBox *chanceSpinner = new QSpinBox;
        int chance = currentField.encounterRates.at(index);
        chanceSpinner->setMinimum(1);
        chanceSpinner->setMaximum(9999);
        chanceSpinner->setValue(chance);
        connect(chanceSpinner, QOverload<int>::of(&QSpinBox::valueChanged), [&updateTotal, &currentField](int) {
            updateTotal(currentField);
        });

        bool useGroups = !currentField.groups.empty();

        QFrame *slotChoiceFrame = new QFrame;
        QVBoxLayout *slotChoiceLayout = new QVBoxLayout;
        if (useGroups) {
            QComboBox *groupCombo = new QComboBox;
            connect(groupCombo, QOverload<const QString &>::of(&QComboBox::textActivated), [&tempFields, &currentField, &updateTotal, index](QString newGroupName) {
                for (EncounterField &field : tempFields) {
                    if (field.name == currentField.name) {
                        for (auto groupNameIterator : field.groups) {
                            QString groupName = groupNameIterator.first;
                            if (field.groups[groupName].contains(index)) {
                                field.groups[groupName].removeAll(index);
                                break;
                            }
                        }
                        for (auto groupNameIterator : field.groups) {
                            QString groupName = groupNameIterator.first;
                            if (groupName == newGroupName) field.groups[newGroupName].append(index);
                        }
                        break;
                    }
                }
                updateTotal(currentField);
            });
            for (auto groupNameIterator : currentField.groups) {
                groupCombo->addItem(groupNameIterator.first);
            }
            QString currentGroupName;
            for (auto groupNameIterator : currentField.groups) {
                QString groupName = groupNameIterator.first;
                if (currentField.groups[groupName].contains(index)) {
                    currentGroupName = groupName;
                    break;
                }
            }
            groupCombo->setCurrentText(currentGroupName);
            slotChoiceLayout->addWidget(groupCombo);
        }
        slotChoiceLayout->addWidget(chanceSpinner);
        slotChoiceFrame->setLayout(slotChoiceLayout);

        QFrame *slot = new QFrame;
        QHBoxLayout *slotLayout = new QHBoxLayout;
        slotLayout->addWidget(indexLabel);
        slotLayout->addWidget(slotChoiceFrame);
        slot->setLayout(slotLayout);

        fieldSlots.append(slot);

        return slot;
    };

    QDialog dialog(window, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    dialog.setWindowTitle("Configure Wild Encounter Fields");
    dialog.setWindowModality(Qt::NonModal);

    QGridLayout grid;

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // lambda: Get a QStringList of the existing field names.
    auto getFieldNames = [&tempFields]() {
        QStringList fieldNames;
        for (EncounterField field : tempFields)
            fieldNames.append(field.name);
        return fieldNames;
    };

    // lambda: Draws the slot widgets onto a grid (4 wide) on the dialog window.
    auto drawSlotWidgets = [&dialog, &grid, &createNewSlot, &fieldSlots, &updateTotal, &tempFields](int index) {
        // Clear them first.
        while (!fieldSlots.isEmpty()) {
            auto slot = fieldSlots.takeFirst();
            grid.removeWidget(slot);
            delete slot;
        }

        if (!tempFields.size()) {
            return;
        }
        if (index >= tempFields.size()) {
            index = tempFields.size() - 1;
        }
        EncounterField &currentField = tempFields[index];
        for (int i = 0; i < currentField.encounterRates.size(); i++) {
            grid.addWidget(createNewSlot(i, currentField), i / 4 + 1, i % 4);
        }

        updateTotal(currentField);

        dialog.adjustSize();// TODO: why is this updating only on second call? reproduce: land->fishing->rock_smash->water
    };
    QComboBox *fieldChoices = new QComboBox;
    connect(fieldChoices, QOverload<int>::of(&QComboBox::currentIndexChanged), drawSlotWidgets);
    fieldChoices->addItems(getFieldNames());

    QLabel *fieldChoiceLabel = new QLabel("Field");

    // Button to create new fields in the JSON.
    QPushButton *addFieldButton = new QPushButton("Add New Field...");
    connect(addFieldButton, &QPushButton::clicked, [fieldChoices, &tempFields]() {
        QDialog newNameDialog(nullptr, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
        newNameDialog.setWindowModality(Qt::NonModal);
        QDialogButtonBox newFieldButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &newNameDialog);
        connect(&newFieldButtonBox, &QDialogButtonBox::accepted, &newNameDialog, &QDialog::accept);
        connect(&newFieldButtonBox, &QDialogButtonBox::rejected, &newNameDialog, &QDialog::reject);

        QLineEdit *newNameEdit = new QLineEdit;
        newNameEdit->setClearButtonEnabled(true);

        QFormLayout newFieldForm(&newNameDialog);

        newFieldForm.addRow("Field Name", newNameEdit);
        newFieldForm.addRow(&newFieldButtonBox);

        if (newNameDialog.exec() == QDialog::Accepted) {
            QString newFieldName = newNameEdit->text();
            QVector<int> newFieldRates(1, 100);
            tempFields.append({newFieldName, newFieldRates, {}});
            fieldChoices->addItem(newFieldName);
            fieldChoices->setCurrentIndex(fieldChoices->count() - 1);
        }
    });
    QPushButton *deleteFieldButton = new QPushButton("Delete Field");
    connect(deleteFieldButton, &QPushButton::clicked, [drawSlotWidgets, fieldChoices, &tempFields]() {
        if (tempFields.size() < 2) return;// don't delete last
        int index = fieldChoices->currentIndex();
        fieldChoices->removeItem(index);
        tempFields.remove(index);
        drawSlotWidgets(index);
    });

    QPushButton *addSlotButton = new QPushButton(QIcon(":/icons/add.ico"), "");
    addSlotButton->setFlat(true);
    connect(addSlotButton, &QPushButton::clicked, [&fieldChoices, &drawSlotWidgets, &tempFields]() {
        EncounterField &field = tempFields[fieldChoices->currentIndex()];
        field.encounterRates.append(1);
        drawSlotWidgets(fieldChoices->currentIndex());
    });
    QPushButton *removeSlotButton = new QPushButton(QIcon(":/icons/delete.ico"), "");
    removeSlotButton->setFlat(true);
    connect(removeSlotButton, &QPushButton::clicked, [&fieldChoices, &drawSlotWidgets, &tempFields]() {
        EncounterField &field = tempFields[fieldChoices->currentIndex()];
        int lastIndex = field.encounterRates.size() - 1;
        if (lastIndex > 0)
            field.encounterRates.removeLast();
        for (auto &g : field.groups) {
            field.groups[g.first].removeAll(lastIndex);
        }
        drawSlotWidgets(fieldChoices->currentIndex());
    });
    // TODO: method for editing groups?

    QFrame firstRow;
    QHBoxLayout firstRowLayout;
    firstRowLayout.addWidget(fieldChoiceLabel);
    firstRowLayout.addWidget(fieldChoices);
    firstRowLayout.addWidget(deleteFieldButton);
    firstRowLayout.addWidget(addFieldButton);
    firstRowLayout.addWidget(removeSlotButton);
    firstRowLayout.addWidget(addSlotButton);
    firstRow.setLayout(&firstRowLayout);
    grid.addWidget(&firstRow, 0, 0, 1, 4, Qt::AlignLeft);

    QHBoxLayout lastRow;
    lastRow.addWidget(totalLabel);
    lastRow.addWidget(&buttonBox);

    // To keep the total and button box at the bottom of the window.
    QVBoxLayout layout(&dialog);
    QFrame *frameTop = new QFrame;
    frameTop->setLayout(&grid);
    layout.addWidget(frameTop);
    QFrame *frameBottom = new QFrame;
    frameBottom->setLayout(&lastRow);
    layout.addWidget(frameBottom);

    if (dialog.exec() == QDialog::Accepted) {
        updateEncounterFields(tempFields);

        // Re-draw the tab accordingly.
        displayWildMonTables();
        saveEncounterTabData();
        emit wildMonTableEdited();
    }
}

void Editor::saveEncounterTabData() {
    // This function does not save to disk so it is safe to use before user clicks Save.
    QStackedWidget *stack = ui->stackedWidget_WildMons;
    QComboBox *labelCombo = ui->comboBox_EncounterGroupLabel;

    if (!stack->count()) return;

    tsl::ordered_map<QString, WildPokemonHeader> &encounterMap = project->wildMonData[map->constantName];

    for (int groupIndex = 0; groupIndex < stack->count(); groupIndex++) {
        MonTabWidget *tabWidget = static_cast<MonTabWidget *>(stack->widget(groupIndex));

        WildPokemonHeader &encounterHeader = encounterMap[labelCombo->itemText(groupIndex)];

        int fieldIndex = 0;
        for (EncounterField monField : project->wildMonFields) {
            QString fieldName = monField.name;

            if (!tabWidget->isTabEnabled(fieldIndex++)) {
                encounterHeader.wildMons.erase(fieldName);
                continue;
            }

            QTableView *monTable = tabWidget->tableAt(fieldIndex - 1);
            EncounterTableModel *model = static_cast<EncounterTableModel *>(monTable->model());
            encounterHeader.wildMons[fieldName] = model->encounterData();
        }
    }
}

EncounterTableModel* Editor::getCurrentWildMonTable() {
    auto tabWidget = static_cast<MonTabWidget*>(ui->stackedWidget_WildMons->currentWidget());
    if (!tabWidget) return nullptr;

    auto tableView = tabWidget->tableAt(tabWidget->currentIndex());
    if (!tableView) return nullptr;

    return static_cast<EncounterTableModel*>(tableView->model());
}

void Editor::updateEncounterFields(EncounterFields newFields) {
    EncounterFields oldFields = project->wildMonFields;
    // Go through fields and determine whether we need to update a field.
    // If the field is new, do nothing.
    // If the field is deleted, remove from all maps.
    // If the field is changed, change all maps accordingly.
    for (EncounterField oldField : oldFields) {
        QString oldFieldName = oldField.name;
        bool fieldDeleted = true;
        for (EncounterField newField : newFields) {
            QString newFieldName = newField.name;
            if (oldFieldName == newFieldName) {
                fieldDeleted = false;
                if (oldField.encounterRates.size() != newField.encounterRates.size()) {
                    for (auto mapPair : project->wildMonData) {
                        QString map = mapPair.first;
                        for (auto groupNamePair : project->wildMonData[map]) {
                            QString groupName = groupNamePair.first;
                            WildPokemonHeader &monHeader = project->wildMonData[map][groupName];
                            for (auto fieldNamePair : monHeader.wildMons) {
                                QString fieldName = fieldNamePair.first;
                                if (fieldName == oldFieldName) {
                                    monHeader.wildMons[fieldName].wildPokemon.resize(newField.encounterRates.size());
                                }
                            }
                        }
                    }
                }
            }
        }
        if (fieldDeleted) {
            for (auto mapPair : project->wildMonData) {
                QString map = mapPair.first;
                for (auto groupNamePair : project->wildMonData[map]) {
                    QString groupName = groupNamePair.first;
                    WildPokemonHeader &monHeader = project->wildMonData[map][groupName];
                    for (auto fieldNamePair : monHeader.wildMons) {
                        QString fieldName = fieldNamePair.first;
                        if (fieldName == oldFieldName) {
                            monHeader.wildMons.erase(fieldName);
                        }
                    }
                }
            }
        }
    }
    project->wildMonFields = newFields;
}

void Editor::disconnectMapConnection(MapConnection *connection) {
    // Disconnect MapConnection's signals used by the display.
    // It'd be nice if we could just 'connection->disconnect(this)' but that doesn't account for lambda functions.
    QObject::disconnect(connection, &MapConnection::targetMapNameChanged, nullptr, nullptr);
    QObject::disconnect(connection, &MapConnection::directionChanged, nullptr, nullptr);
    QObject::disconnect(connection, &MapConnection::offsetChanged, nullptr, nullptr);
}

void Editor::displayConnection(MapConnection *connection) {
    if (!connection)
        return;

    if (MapConnection::isDiving(connection->direction())) {
        displayDivingConnection(connection);
        return;
    }

    // Create connection image
    ConnectionPixmapItem *pixmapItem = new ConnectionPixmapItem(connection, getConnectionOrigin(connection));
    pixmapItem->render();
    scene->addItem(pixmapItem);
    maskNonVisibleConnectionTiles();

    // Create item for the list panel
    ConnectionsListItem *listItem = new ConnectionsListItem(ui->scrollAreaContents_ConnectionsList, pixmapItem->connection, project->mapNames);
    ui->layout_ConnectionsList->insertWidget(ui->layout_ConnectionsList->count() - 1, listItem); // Insert above the vertical spacer

    // Double clicking the pixmap or clicking the list item's map button opens the connected map
    connect(listItem, &ConnectionsListItem::openMapClicked, this, &Editor::openConnectedMap);
    connect(pixmapItem, &ConnectionPixmapItem::connectionItemDoubleClicked, this, &Editor::openConnectedMap);

    // Sync the selection highlight between the list UI and the pixmap
    connect(pixmapItem, &ConnectionPixmapItem::selectionChanged, [=](bool selected) {
        listItem->setSelected(selected);
        if (selected) setSelectedConnectionItem(pixmapItem);
    });
    connect(listItem, &ConnectionsListItem::selected, [=] {
        setSelectedConnectionItem(pixmapItem);
    });

    // Sync edits to 'offset' between the list UI and the pixmap
    connect(connection, &MapConnection::offsetChanged, [=](int, int) {
        listItem->updateUI();
        pixmapItem->updatePos();
        maskNonVisibleConnectionTiles();
    });

    // Sync edits to 'direction' between the list UI and the pixmap
    connect(connection, &MapConnection::directionChanged, [=](QString, QString) {
        listItem->updateUI();
        updateConnectionPixmap(pixmapItem);
    });

    // Sync edits to 'map' between the list UI and the pixmap
    connect(connection, &MapConnection::targetMapNameChanged, [=](QString, QString) {
        listItem->updateUI();
        updateConnectionPixmap(pixmapItem);
    });

    // When the pixmap is deleted, remove its associated list item
    connect(pixmapItem, &ConnectionPixmapItem::destroyed, listItem, &ConnectionsListItem::deleteLater);

    connection_items.append(pixmapItem);

    // If this was a recent addition from the user we should select it.
    // We intentionally exclude connections added programmatically, e.g. by mirroring.
    if (connection_to_select == connection) {
        connection_to_select = nullptr;
        setSelectedConnectionItem(pixmapItem);
    }
}

void Editor::addConnection(MapConnection *connection) {
    if (!connection)
        return;

    // Mark this connection to be selected once its display elements have been created.
    // It's possible this is a Dive/Emerge connection, but that's ok (no selection will occur).
    connection_to_select = connection;

    this->map->editHistory.push(new MapConnectionAdd(this->map, connection));
}

void Editor::removeConnection(MapConnection *connection) {
    if (!connection)
        return;
    this->map->editHistory.push(new MapConnectionRemove(this->map, connection));
}

void Editor::removeSelectedConnection() {
    if (selected_connection_item)
        removeConnection(selected_connection_item->connection);
}

void Editor::removeConnectionPixmap(MapConnection *connection) {
    if (!connection)
        return;

    disconnectMapConnection(connection);

    if (MapConnection::isDiving(connection->direction())) {
        removeDivingMapPixmap(connection);
        return;
    }

    int i;
    for (i = 0; i < connection_items.length(); i++) {
        if (connection_items.at(i)->connection == connection)
            break;
    }
    if (i == connection_items.length())
        return; // Connection is not displayed, nothing to do.

    auto pixmapItem = connection_items.takeAt(i);
    if (pixmapItem == selected_connection_item) {
        // This was the selected connection, select the next one up in the list.
        selected_connection_item = nullptr;
        if (i != 0) i--;
        if (connection_items.length() > i)
            setSelectedConnectionItem(connection_items.at(i));
    }

    if (pixmapItem->scene())
        pixmapItem->scene()->removeItem(pixmapItem);

    delete pixmapItem;
}

void Editor::displayDivingConnection(MapConnection *connection) {
    if (!connection)
        return;

    const QString direction = connection->direction();
    if (!MapConnection::isDiving(direction))
        return;

    // Note: We only support editing 1 Dive and Emerge connection per map.
    //       In a vanilla game only the first Dive/Emerge connection is considered, so allowing
    //       users to have multiple is likely to lead to confusion. In case users have changed
    //       this we won't delete extra diving connections, but we'll only display the first one.
    if (diving_map_items.value(direction))
        return;

    // Create map display
    auto comboBox = (direction == "dive") ? ui->comboBox_DiveMap : ui->comboBox_EmergeMap;
    auto item = new DivingMapPixmapItem(connection, comboBox);
    scene->addItem(item);
    diving_map_items.insert(direction, item);

    updateDivingMapsVisibility();
}

void Editor::renderDivingConnections() {
    for (auto item : diving_map_items.values())
        item->updatePixmap();
}

void Editor::removeDivingMapPixmap(MapConnection *connection) {
    if (!connection)
        return;

    const QString direction = connection->direction();
    if (!diving_map_items.contains(direction))
        return;

    // If the diving map being removed is different than the one that's currently displayed we don't need to do anything.
    if (diving_map_items.value(direction)->connection() != connection)
        return;

    // Delete map image
    auto pixmapItem = diving_map_items.take(direction);
    if (pixmapItem->scene())
        pixmapItem->scene()->removeItem(pixmapItem);
    delete pixmapItem;

    // Reveal any previously-hidden connection (because we only ever display one diving map of each type).
    // Note: When this occurs as a result of the user clicking the 'X' clear button it seems the QComboBox
    //       doesn't expect the line edit to be immediately repopulated, and the 'X' doesn't reappear.
    //       As a workaround we wait before displaying the new text. The wait time is essentially arbitrary.
    for (auto i : map->getConnections()) {
        if (i->direction() == direction) {
            QTimer::singleShot(10, Qt::CoarseTimer, [this, i]() { displayDivingConnection(i); });
            break;
        }
    }
    updateDivingMapsVisibility();
}

void Editor::updateDiveMap(QString mapName) {
    setDivingMapName(mapName, "dive");
}

void Editor::updateEmergeMap(QString mapName) {
    setDivingMapName(mapName, "emerge");
}

void Editor::setDivingMapName(QString mapName, QString direction) {
    auto pixmapItem = diving_map_items.value(direction);
    MapConnection *connection = pixmapItem ? pixmapItem->connection() : nullptr;

    if (connection) {
        if (mapName == connection->targetMapName())
            return; // No change

        // Update existing connection
        if (mapName.isEmpty()) {
            removeConnection(connection);
        } else {
            map->editHistory.push(new MapConnectionChangeMap(connection, mapName));
        }
    } else if (!mapName.isEmpty()) {
        // Create new connection
        addConnection(new MapConnection(mapName, direction));
    }
}

void Editor::updateDivingMapsVisibility() {
    auto dive = diving_map_items.value("dive");
    auto emerge = diving_map_items.value("emerge");

    if (dive && emerge) {
        // Both connections in use, use separate sliders
        ui->stackedWidget_DiveMapOpacity->setCurrentIndex(0);
        dive->setOpacity(!porymapConfig.showDiveEmergeMaps ? 0 : static_cast<qreal>(porymapConfig.diveMapOpacity) / 100);
        emerge->setOpacity(!porymapConfig.showDiveEmergeMaps ? 0 : static_cast<qreal>(porymapConfig.emergeMapOpacity) / 100);
    } else {
        // One connection in use (or none), use single slider
        ui->stackedWidget_DiveMapOpacity->setCurrentIndex(1);
        qreal opacity = !porymapConfig.showDiveEmergeMaps ? 0 : static_cast<qreal>(porymapConfig.diveEmergeMapOpacity) / 100;
        if (dive) dive->setOpacity(opacity);
        else if (emerge) emerge->setOpacity(opacity);
    }
}

// Get the 'origin' point for the connection's pixmap, i.e. where it should be positioned in the editor when connection->offset() == 0.
// This differs depending on the connection's direction and the dimensions of its target map or parent map.
QPoint Editor::getConnectionOrigin(MapConnection *connection) {
    if (!connection)
        return QPoint(0, 0);

    Map *parentMap = connection->parentMap();
    Map *targetMap = connection->targetMap();
    const QString direction = connection->direction();
    int x = 0, y = 0;

    if (direction == "right") {
        if (parentMap) x = parentMap->getWidth();
    } else if (direction == "down") {
        if (parentMap) y = parentMap->getHeight();
    } else if (direction == "left") {
        if (targetMap) x = -targetMap->getConnectionRect(direction).width();
    } else if (direction == "up") {
        if (targetMap) y = -targetMap->getConnectionRect(direction).height();
    }
    return QPoint(x * 16, y * 16);
}

void Editor::updateConnectionPixmap(ConnectionPixmapItem *pixmapItem) {
    if (!pixmapItem)
        return;

    pixmapItem->setOrigin(getConnectionOrigin(pixmapItem->connection));
    pixmapItem->render(true); // Full render to reflect map changes

    maskNonVisibleConnectionTiles();
}

void Editor::setSelectedConnectionItem(ConnectionPixmapItem *pixmapItem) {
    if (!pixmapItem || pixmapItem == selected_connection_item)
        return;

    if (selected_connection_item) selected_connection_item->setSelected(false);
    selected_connection_item = pixmapItem;
    selected_connection_item->setSelected(true);
}

void Editor::setSelectedConnection(MapConnection *connection) {
    if (!connection)
        return;

    for (auto item : connection_items) {
        if (item->connection == connection) {
            setSelectedConnectionItem(item);
            break;
        }
    }
}

void Editor::onBorderMetatilesChanged() {
    displayMapBorder();
    updateBorderVisibility();
}

void Editor::onHoveredMovementPermissionChanged(uint16_t collision, uint16_t elevation) {
    this->ui->statusBar->showMessage(this->getMovementPermissionText(collision, elevation));
}

void Editor::onHoveredMovementPermissionCleared() {
    this->ui->statusBar->clearMessage();
}

QString Editor::getMetatileDisplayMessage(uint16_t metatileId) {
    Metatile *metatile = Tileset::getMetatile(metatileId, map->layout->tileset_primary, map->layout->tileset_secondary);
    QString label = Tileset::getMetatileLabel(metatileId, map->layout->tileset_primary, map->layout->tileset_secondary);
    QString message = QString("Metatile: %1").arg(Metatile::getMetatileIdString(metatileId));
    if (label.size())
        message += QString(" \"%1\"").arg(label);
    if (metatile && metatile->behavior() != 0) { // Skip MB_NORMAL
        const QString behaviorStr = this->project->metatileBehaviorMapInverse.value(metatile->behavior(), "0x" + QString::number(metatile->behavior(), 16));
        message += QString(", Behavior: %1").arg(behaviorStr);
    }
    return message;
}

void Editor::onHoveredMetatileSelectionChanged(uint16_t metatileId) {
    this->ui->statusBar->showMessage(getMetatileDisplayMessage(metatileId));
}

void Editor::onHoveredMetatileSelectionCleared() {
    this->ui->statusBar->clearMessage();
}

void Editor::onSelectedMetatilesChanged() {
    QPoint size = this->metatile_selector_item->getSelectionDimensions();
    this->cursorMapTileRect->updateSelectionSize(size.x(), size.y());
    this->redrawCurrentMetatilesSelection();
}

void Editor::onWheelZoom(int s) {
    // Don't zoom the map when the user accidentally scrolls while performing a magic fill. (ctrl + middle button click)
    if (!(QApplication::mouseButtons() & Qt::MiddleButton)) {
        scaleMapView(s);
    }
}

const QList<double> zoomLevels = QList<double>
{
    0.5,
    0.75,
    1.0,
    1.5,
    2.0,
    3.0,
    4.0,
    6.0,
};

void Editor::scaleMapView(int s) {
    // Clamp the scale index to a valid value.
    int nextScaleIndex = this->scaleIndex + s;
    if (nextScaleIndex < 0)
        nextScaleIndex = 0;
    if (nextScaleIndex >= zoomLevels.size())
        nextScaleIndex = zoomLevels.size() - 1;

    // Early exit if the scale index hasn't changed.
    if (nextScaleIndex == this->scaleIndex)
        return;

    // Set the graphics views' scale transformation based
    // on the new scale amount.
    this->scaleIndex = nextScaleIndex;
    double scaleFactor = zoomLevels[nextScaleIndex];
    QTransform transform = QTransform::fromScale(scaleFactor, scaleFactor);
    ui->graphicsView_Map->setTransform(transform);
    ui->graphicsView_Connections->setTransform(transform);
}

void Editor::updateCursorRectPos(int x, int y) {
    if (this->playerViewRect)
        this->playerViewRect->updateLocation(x, y);
    if (this->cursorMapTileRect)
        this->cursorMapTileRect->updateLocation(x, y);
    if (ui->graphicsView_Map->scene())
        ui->graphicsView_Map->scene()->update();
}

void Editor::setCursorRectVisible(bool visible) {
    if (this->playerViewRect)
        this->playerViewRect->setVisible(visible);
    if (this->cursorMapTileRect)
        this->cursorMapTileRect->setVisible(visible);
    if (ui->graphicsView_Map->scene())
        ui->graphicsView_Map->scene()->update();
}

void Editor::onHoveredMapMetatileChanged(const QPoint &pos) {
    int x = pos.x();
    int y = pos.y();
    if (!map->isWithinBounds(x, y))
        return;

    this->updateCursorRectPos(x, y);
    if (map_item->paintingMode == MapPixmapItem::PaintMode::Metatiles) {
        int blockIndex = y * map->getWidth() + x;
        int metatileId = map->layout->blockdata.at(blockIndex).metatileId();
        this->ui->statusBar->showMessage(QString("X: %1, Y: %2, %3, Scale = %4x")
                              .arg(x)
                              .arg(y)
                              .arg(getMetatileDisplayMessage(metatileId))
                              .arg(QString::number(zoomLevels[this->scaleIndex], 'g', 2)));
    }
    else if (map_item->paintingMode == MapPixmapItem::PaintMode::Events) {
        this->ui->statusBar->showMessage(QString("X: %1, Y: %2, Scale = %3x")
                              .arg(x)
                              .arg(y)
                              .arg(QString::number(zoomLevels[this->scaleIndex], 'g', 2)));
    }
    Scripting::cb_BlockHoverChanged(x, y);
}

void Editor::onHoveredMapMetatileCleared() {
    this->setCursorRectVisible(false);
    if (map_item->paintingMode == MapPixmapItem::PaintMode::Metatiles
     || map_item->paintingMode == MapPixmapItem::PaintMode::Events) {
        this->ui->statusBar->clearMessage();
    }
    Scripting::cb_BlockHoverCleared();
}

void Editor::onHoveredMapMovementPermissionChanged(int x, int y) {
    if (!map->isWithinBounds(x, y))
        return;

    this->updateCursorRectPos(x, y);
    if (map_item->paintingMode == MapPixmapItem::PaintMode::Metatiles) {
        int blockIndex = y * map->getWidth() + x;
        uint16_t collision = map->layout->blockdata.at(blockIndex).collision();
        uint16_t elevation = map->layout->blockdata.at(blockIndex).elevation();
        QString message = QString("X: %1, Y: %2, %3")
                            .arg(x)
                            .arg(y)
                            .arg(this->getMovementPermissionText(collision, elevation));
        this->ui->statusBar->showMessage(message);
    }
    Scripting::cb_BlockHoverChanged(x, y);
}

void Editor::onHoveredMapMovementPermissionCleared() {
    this->setCursorRectVisible(false);
    if (map_item->paintingMode == MapPixmapItem::PaintMode::Metatiles) {
        this->ui->statusBar->clearMessage();
    }
    Scripting::cb_BlockHoverCleared();
}

QString Editor::getMovementPermissionText(uint16_t collision, uint16_t elevation) {
    QString message;
    if (collision != 0) {
        message = QString("Collision: Impassable (%1), Elevation: %2").arg(collision).arg(elevation);
    } else if (elevation == 0) {
        message = "Collision: Transition between elevations";
    } else if (elevation == 15) {
        message = "Collision: Multi-Level (Bridge)";
    } else if (elevation == 1) {
        message = "Collision: Surf";
    } else {
        message = QString("Collision: Passable, Elevation: %1").arg(elevation);
    }
    return message;
}

bool Editor::setMap(QString map_name) {
    if (map_name.isEmpty()) {
        return false;
    }

    // disconnect previous map's signals so they are not firing
    // multiple times if set again in the future
    if (map) {
        map->pruneEditHistory();
        map->disconnect(this);
        for (auto connection : map->getConnections())
            disconnectMapConnection(connection);
    }

    if (project) {
        Map *loadedMap = project->loadMap(map_name);
        if (!loadedMap) {
            return false;
        }

        map = loadedMap;

        editGroup.addStack(&map->editHistory);
        editGroup.setActiveStack(&map->editHistory);
        selected_events->clear();
        if (!displayMap()) {
            return false;
        }
        map_ruler->setMapDimensions(QSize(map->getWidth(), map->getHeight()));
        connect(map, &Map::mapDimensionsChanged, map_ruler, &MapRuler::setMapDimensions);
        connect(map, &Map::openScriptRequested, this, &Editor::openScript);
        connect(map, &Map::connectionAdded, this, &Editor::displayConnection);
        connect(map, &Map::connectionRemoved, this, &Editor::removeConnectionPixmap);
        updateSelectedEvents();
    }

    return true;
}

void Editor::onMapStartPaint(QGraphicsSceneMouseEvent *event, MapPixmapItem *item) {
    if (item->paintingMode != MapPixmapItem::PaintMode::Metatiles) {
        return;
    }

    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    if (event->buttons() & Qt::RightButton && (map_edit_mode == "paint" || map_edit_mode == "fill")) {
        this->cursorMapTileRect->initRightClickSelectionAnchor(pos.x(), pos.y());
    } else {
        this->cursorMapTileRect->initAnchor(pos.x(), pos.y());
    }
}

void Editor::onMapEndPaint(QGraphicsSceneMouseEvent *, MapPixmapItem *item) {
    if (!(item->paintingMode == MapPixmapItem::PaintMode::Metatiles)) {
        return;
    }
    this->cursorMapTileRect->stopRightClickSelectionAnchor();
    this->cursorMapTileRect->stopAnchor();
}

void Editor::setSmartPathCursorMode(QGraphicsSceneMouseEvent *event)
{
    bool shiftPressed = event->modifiers() & Qt::ShiftModifier;
    if (settings->smartPathsEnabled) {
        if (!shiftPressed) {
            this->cursorMapTileRect->setSmartPathMode(true);
        } else {
            this->cursorMapTileRect->setSmartPathMode(false);
        }
    } else {
        if (shiftPressed) {
            this->cursorMapTileRect->setSmartPathMode(true);
        } else {
            this->cursorMapTileRect->setSmartPathMode(false);
        }
    }
}

void Editor::setStraightPathCursorMode(QGraphicsSceneMouseEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        this->cursorMapTileRect->setStraightPathMode(true);
    } else {
        this->cursorMapTileRect->setStraightPathMode(false);
    }
}

void Editor::mouseEvent_map(QGraphicsSceneMouseEvent *event, MapPixmapItem *item) {
    // TODO: add event tab painting tool buttons stuff here
    if (item->paintingMode == MapPixmapItem::PaintMode::Disabled) {
        return;
    }

    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());

    if (item->paintingMode == MapPixmapItem::PaintMode::Metatiles) {
        if (map_edit_mode == "paint") {
            if (event->buttons() & Qt::RightButton) {
                item->updateMetatileSelection(event);
            } else if (event->buttons() & Qt::MiddleButton) {
                if (event->modifiers() & Qt::ControlModifier) {
                    item->magicFill(event);
                } else {
                    item->floodFill(event);
                }
            } else {
                if (event->type() == QEvent::GraphicsSceneMouseRelease) {
                    // Update the tile rectangle at the end of a click-drag selection
                    this->updateCursorRectPos(pos.x(), pos.y());
                }
                this->setSmartPathCursorMode(event);
                this->setStraightPathCursorMode(event);
                if (this->cursorMapTileRect->getStraightPathMode()) {
                    item->lockNondominantAxis(event);
                    pos = item->adjustCoords(pos);
                }
                item->paint(event);
            }
        } else if (map_edit_mode == "select") {
            item->select(event);
        } else if (map_edit_mode == "fill") {
            if (event->buttons() & Qt::RightButton) {
                item->updateMetatileSelection(event);
            } else if (event->modifiers() & Qt::ControlModifier) {
                item->magicFill(event);
            } else {
                item->floodFill(event);
            }
        } else if (map_edit_mode == "pick") {
            if (event->buttons() & Qt::RightButton) {
                item->updateMetatileSelection(event);
            } else {
                item->pick(event);
            }
        } else if (map_edit_mode == "shift") {
            this->setStraightPathCursorMode(event);
            if (this->cursorMapTileRect->getStraightPathMode()) {
                item->lockNondominantAxis(event);
                pos = item->adjustCoords(pos);
            }
            item->shift(event);
        }
    } else if (item->paintingMode == MapPixmapItem::PaintMode::Events) {
        if (obj_edit_mode == "paint" && event->type() == QEvent::GraphicsSceneMousePress) {
            // Right-clicking while in paint mode will change mode to select.
            if (event->buttons() & Qt::RightButton) {
                this->obj_edit_mode = "select";
                this->settings->mapCursor = QCursor();
                this->cursorMapTileRect->setSingleTileMode();
                this->ui->toolButton_Paint->setChecked(false);
                this->ui->toolButton_Select->setChecked(true);
            } else {
                // Left-clicking while in paint mode will add a new event of the
                // type of the first currently selected events.
                Event::Type eventType = Event::Type::Object;
                if (this->selected_events->size() > 0)
                    eventType = this->selected_events->first()->event->getEventType();

                if (eventType == Event::Type::HealLocation && !porymapConfig.allowHealLocationDeleting) {
                    // Can't freely add Heal Locations if deleting them is not enabled.
                    return;
                }

                DraggablePixmapItem *newEvent = addNewEvent(eventType);
                if (newEvent) {
                    newEvent->move(pos.x(), pos.y());
                    selectMapEvent(newEvent);
                }
            }
        } else if (obj_edit_mode == "select") {
            // do nothing here, at least for now
        } else if (obj_edit_mode == "shift" && item->map) {
            static QPoint selection_origin;
            static unsigned actionId = 0;

            if (event->type() == QEvent::GraphicsSceneMouseRelease) {
                actionId++;
            } else {
                if (event->type() == QEvent::GraphicsSceneMousePress) {
                    selection_origin = QPoint(pos.x(), pos.y());
                } else if (event->type() == QEvent::GraphicsSceneMouseMove) {
                    if (pos.x() != selection_origin.x() || pos.y() != selection_origin.y()) {
                        int xDelta = pos.x() - selection_origin.x();
                        int yDelta = pos.y() - selection_origin.y();

                        QList<Event *> selectedEvents;

                        for (const auto &item : getEventPixmapItems()) {
                            selectedEvents.append(item->event);
                        }
                        selection_origin = QPoint(pos.x(), pos.y());

                        map->editHistory.push(new EventShift(selectedEvents, xDelta, yDelta, actionId));
                    }
                }
            }
        }
    }
}

void Editor::mouseEvent_collision(QGraphicsSceneMouseEvent *event, CollisionPixmapItem *item) {
    if (item->paintingMode != MapPixmapItem::PaintMode::Metatiles) {
        return;
    }

    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());

    if (map_edit_mode == "paint") {
        if (event->buttons() & Qt::RightButton) {
            item->updateMovementPermissionSelection(event);
        } else if (event->buttons() & Qt::MiddleButton) {
            if (event->modifiers() & Qt::ControlModifier) {
                item->magicFill(event);
            } else {
                item->floodFill(event);
            }
        } else {
            this->setStraightPathCursorMode(event);
            if (this->cursorMapTileRect->getStraightPathMode()) {
                item->lockNondominantAxis(event);
                pos = item->adjustCoords(pos);
            }
            item->paint(event);
        }
    } else if (map_edit_mode == "select") {
        item->select(event);
    } else if (map_edit_mode == "fill") {
        if (event->buttons() & Qt::RightButton) {
            item->pick(event);
        } else if (event->modifiers() & Qt::ControlModifier) {
            item->magicFill(event);
        } else {
            item->floodFill(event);
        }
    } else if (map_edit_mode == "pick") {
        item->pick(event);
    } else if (map_edit_mode == "shift") {
        this->setStraightPathCursorMode(event);
        if (this->cursorMapTileRect->getStraightPathMode()) {
            item->lockNondominantAxis(event);
            pos = item->adjustCoords(pos);
        }
        item->shift(event);
    }
}

// On project close we want to leave the editor view empty.
// Otherwise a map is normally only cleared when a new one is being displayed.
void Editor::clearMap() {
    clearMetatileSelector();
    clearMovementPermissionSelector();
    clearMapMetatiles();
    clearMapMovementPermissions();
    clearBorderMetatiles();
    clearCurrentMetatilesSelection();
    clearMapEvents();
    clearMapConnections();
    clearMapBorder();
    clearMapGrid();
    clearWildMonTables();
    clearConnectionMask();

    // Clear pointers to objects deleted elsewhere
    current_view = nullptr;
    map = nullptr;

    // These are normally preserved between map displays, we only delete them now.
    delete scene;
    delete metatile_selector_item;
    delete movement_permissions_selector_item;
}

bool Editor::displayMap() {
    if (!scene) {
        scene = new QGraphicsScene;
        MapSceneEventFilter *filter = new MapSceneEventFilter(scene);
        scene->installEventFilter(filter);
        connect(filter, &MapSceneEventFilter::wheelZoom, this, &Editor::onWheelZoom);
        scene->installEventFilter(this->map_ruler);
    }

    displayMetatileSelector();
    displayMovementPermissionSelector();
    displayMapMetatiles();
    displayMapMovementPermissions();
    displayBorderMetatiles();
    displayCurrentMetatilesSelection();
    displayMapEvents();
    displayMapConnections();
    displayMapBorder();
    displayMapGrid();
    displayWildMonTables();
    maskNonVisibleConnectionTiles();

    this->map_ruler->setZValue(1000);
    scene->addItem(this->map_ruler);

    if (map_item) {
        map_item->setVisible(false);
    }
    if (collision_item) {
        collision_item->setVisible(false);
    }
    if (events_group) {
        events_group->setVisible(false);
    }
    return true;
}

void Editor::clearMetatileSelector() {
    if (metatile_selector_item && metatile_selector_item->scene()) {
        metatile_selector_item->scene()->removeItem(metatile_selector_item);
        delete scene_metatiles;
    }
}

void Editor::displayMetatileSelector() {
    clearMetatileSelector();

    scene_metatiles = new QGraphicsScene;
    if (!metatile_selector_item) {
        metatile_selector_item = new MetatileSelector(8, map);
        connect(metatile_selector_item, &MetatileSelector::hoveredMetatileSelectionChanged,
                this, &Editor::onHoveredMetatileSelectionChanged);
        connect(metatile_selector_item, &MetatileSelector::hoveredMetatileSelectionCleared,
                this, &Editor::onHoveredMetatileSelectionCleared);
        connect(metatile_selector_item, &MetatileSelector::selectedMetatilesChanged,
                this, &Editor::onSelectedMetatilesChanged);
        metatile_selector_item->select(0);
    } else {
        metatile_selector_item->setMap(map);
        if (metatile_selector_item->primaryTileset
         && metatile_selector_item->primaryTileset != map->layout->tileset_primary)
            emit tilesetUpdated(map->layout->tileset_primary->name);
        if (metatile_selector_item->secondaryTileset
         && metatile_selector_item->secondaryTileset != map->layout->tileset_secondary)
            emit tilesetUpdated(map->layout->tileset_secondary->name);
        metatile_selector_item->setTilesets(map->layout->tileset_primary, map->layout->tileset_secondary);
    }

    scene_metatiles->addItem(metatile_selector_item);
}

void Editor::clearMapMetatiles() {
    if (map_item && scene) {
        scene->removeItem(map_item);
        delete map_item;
        scene->removeItem(this->map_ruler);
    }
}

void Editor::displayMapMetatiles() {
    clearMapMetatiles();

    map_item = new MapPixmapItem(map, this->metatile_selector_item, this->settings);
    connect(map_item, &MapPixmapItem::mouseEvent, this, &Editor::mouseEvent_map);
    connect(map_item, &MapPixmapItem::startPaint, this, &Editor::onMapStartPaint);
    connect(map_item, &MapPixmapItem::endPaint, this, &Editor::onMapEndPaint);
    connect(map_item, &MapPixmapItem::hoveredMapMetatileChanged, this, &Editor::onHoveredMapMetatileChanged);
    connect(map_item, &MapPixmapItem::hoveredMapMetatileCleared, this, &Editor::onHoveredMapMetatileCleared);

    map_item->draw(true);
    scene->addItem(map_item);

    int tw = 16;
    int th = 16;
    scene->setSceneRect(
        -BORDER_DISTANCE * tw,
        -BORDER_DISTANCE * th,
        map_item->pixmap().width() + BORDER_DISTANCE * 2 * tw,
        map_item->pixmap().height() + BORDER_DISTANCE * 2 * th
    );
}

void Editor::clearMapMovementPermissions() {
    if (collision_item && scene) {
        scene->removeItem(collision_item);
        delete collision_item;
    }
}

void Editor::displayMapMovementPermissions() {
    clearMapMovementPermissions();

    collision_item = new CollisionPixmapItem(map, ui->spinBox_SelectedCollision, ui->spinBox_SelectedElevation,
                                             this->metatile_selector_item, this->settings, &this->collisionOpacity);
    connect(collision_item, &CollisionPixmapItem::mouseEvent, this, &Editor::mouseEvent_collision);
    connect(collision_item, &CollisionPixmapItem::hoveredMapMovementPermissionChanged,
            this, &Editor::onHoveredMapMovementPermissionChanged);
    connect(collision_item, &CollisionPixmapItem::hoveredMapMovementPermissionCleared,
            this, &Editor::onHoveredMapMovementPermissionCleared);

    collision_item->draw(true);
    scene->addItem(collision_item);
}

void Editor::clearBorderMetatiles() {
    if (selected_border_metatiles_item && selected_border_metatiles_item->scene()) {
        selected_border_metatiles_item->scene()->removeItem(selected_border_metatiles_item);
        delete selected_border_metatiles_item;
        delete scene_selected_border_metatiles;
    }
}

void Editor::displayBorderMetatiles() {
    clearBorderMetatiles();

    scene_selected_border_metatiles = new QGraphicsScene;
    selected_border_metatiles_item = new BorderMetatilesPixmapItem(map, this->metatile_selector_item);
    selected_border_metatiles_item->draw();
    scene_selected_border_metatiles->addItem(selected_border_metatiles_item);

    connect(selected_border_metatiles_item, &BorderMetatilesPixmapItem::hoveredBorderMetatileSelectionChanged,
            this, &Editor::onHoveredMetatileSelectionChanged);
    connect(selected_border_metatiles_item, &BorderMetatilesPixmapItem::hoveredBorderMetatileSelectionCleared,
            this, &Editor::onHoveredMetatileSelectionCleared);
    connect(selected_border_metatiles_item, &BorderMetatilesPixmapItem::borderMetatilesChanged,
            this, &Editor::onBorderMetatilesChanged);
}

void Editor::clearCurrentMetatilesSelection() {
    if (current_metatile_selection_item && current_metatile_selection_item->scene()) {
        current_metatile_selection_item->scene()->removeItem(current_metatile_selection_item);
        delete current_metatile_selection_item;
        current_metatile_selection_item = nullptr;
        delete scene_current_metatile_selection;
    }
}

void Editor::displayCurrentMetatilesSelection() {
    clearCurrentMetatilesSelection();

    scene_current_metatile_selection = new QGraphicsScene;
    current_metatile_selection_item = new CurrentSelectedMetatilesPixmapItem(map, this->metatile_selector_item);
    current_metatile_selection_item->draw();
    scene_current_metatile_selection->addItem(current_metatile_selection_item);
}

void Editor::redrawCurrentMetatilesSelection() {
    if (current_metatile_selection_item) {
        current_metatile_selection_item->setMap(map);
        current_metatile_selection_item->draw();
        emit currentMetatilesSelectionChanged();
    }
}

void Editor::clearMovementPermissionSelector() {
    if (movement_permissions_selector_item && movement_permissions_selector_item->scene()) {
        movement_permissions_selector_item->scene()->removeItem(movement_permissions_selector_item);
        delete scene_collision_metatiles;
    }
}

void Editor::displayMovementPermissionSelector() {
    clearMovementPermissionSelector();

    scene_collision_metatiles = new QGraphicsScene;
    if (!movement_permissions_selector_item) {
        movement_permissions_selector_item = new MovementPermissionsSelector(this->collisionSheetPixmap);
        connect(movement_permissions_selector_item, &MovementPermissionsSelector::hoveredMovementPermissionChanged,
                this, &Editor::onHoveredMovementPermissionChanged);
        connect(movement_permissions_selector_item, &MovementPermissionsSelector::hoveredMovementPermissionCleared,
                this, &Editor::onHoveredMovementPermissionCleared);
        connect(movement_permissions_selector_item, &SelectablePixmapItem::selectionChanged, [this](int x, int y, int, int) {
            this->setCollisionTabSpinBoxes(x, y);
        });
        movement_permissions_selector_item->select(projectConfig.defaultCollision, projectConfig.defaultElevation);
    }

    scene_collision_metatiles->addItem(movement_permissions_selector_item);
}

void Editor::clearMapEvents() {
    if (events_group) {
        for (QGraphicsItem *child : events_group->childItems()) {
            events_group->removeFromGroup(child);
            delete child;
        }

        if (events_group->scene()) {
            events_group->scene()->removeItem(events_group);
        }

        delete events_group;
        events_group = nullptr;
    }
    selected_events->clear();
}

void Editor::displayMapEvents() {
    clearMapEvents();

    events_group = new QGraphicsItemGroup;
    scene->addItem(events_group);

    QList<Event *> events = map->getAllEvents();
    for (Event *event : events) {
        project->setEventPixmap(event);
        addMapEvent(event);
    }
    //objects_group->setFiltersChildEvents(false);
    events_group->setHandlesChildEvents(false);
}

DraggablePixmapItem *Editor::addMapEvent(Event *event) {
    DraggablePixmapItem *item = new DraggablePixmapItem(event, this);
    redrawEventPixmapItem(item);
    events_group->addToGroup(item);
    return item;
}

void Editor::clearMapConnections() {
    for (auto item : connection_items) {
        if (item->scene())
            item->scene()->removeItem(item);
        delete item;
    }
    connection_items.clear();

    const QSignalBlocker blocker1(ui->comboBox_DiveMap);
    const QSignalBlocker blocker2(ui->comboBox_EmergeMap);
    ui->comboBox_DiveMap->setCurrentText("");
    ui->comboBox_EmergeMap->setCurrentText("");

    for (auto item : diving_map_items.values()) {
        if (item->scene())
            item->scene()->removeItem(item);
        delete item;
    }
    diving_map_items.clear();

    // Reset to single opacity slider
    ui->stackedWidget_DiveMapOpacity->setCurrentIndex(1);

    selected_connection_item = nullptr;
}

void Editor::displayMapConnections() {
    clearMapConnections();

    for (auto connection : map->getConnections())
        displayConnection(connection);

    if (!connection_items.isEmpty())
        setSelectedConnectionItem(connection_items.first());
}

void Editor::clearConnectionMask() {
    if (connection_mask) {
        if (connection_mask->scene()) {
            connection_mask->scene()->removeItem(connection_mask);
        }
        delete connection_mask;
        connection_mask = nullptr;
    }
}

// Hides connected map tiles that cannot be seen from the current map (beyond BORDER_DISTANCE).
void Editor::maskNonVisibleConnectionTiles() {
    clearConnectionMask();

    QPainterPath mask;
    mask.addRect(scene->itemsBoundingRect().toRect());
    mask.addRect(
        -BORDER_DISTANCE * 16,
        -BORDER_DISTANCE * 16,
        (map->getWidth() + BORDER_DISTANCE * 2) * 16,
        (map->getHeight() + BORDER_DISTANCE * 2) * 16
    );

    // Mask the tiles with the current theme's background color.
    QPen pen(ui->graphicsView_Map->palette().color(QPalette::Active, QPalette::Base));
    QBrush brush(ui->graphicsView_Map->palette().color(QPalette::Active, QPalette::Base));

    connection_mask = scene->addPath(mask, pen, brush);
}

void Editor::clearMapBorder() {
    for (QGraphicsPixmapItem* item : borderItems) {
        if (item->scene()) {
            item->scene()->removeItem(item);
        }
        delete item;
    }
    borderItems.clear();
}

void Editor::displayMapBorder() {
    clearMapBorder();

    int borderWidth = map->getBorderWidth();
    int borderHeight = map->getBorderHeight();
    int borderHorzDist = getBorderDrawDistance(borderWidth);
    int borderVertDist = getBorderDrawDistance(borderHeight);
    QPixmap pixmap = map->renderBorder();
    for (int y = -borderVertDist; y < map->getHeight() + borderVertDist; y += borderHeight)
    for (int x = -borderHorzDist; x < map->getWidth() + borderHorzDist; x += borderWidth) {
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(pixmap);
        item->setX(x * 16);
        item->setY(y * 16);
        item->setZValue(-3);
        scene->addItem(item);
        borderItems.append(item);
    }
}

void Editor::updateMapBorder() {
    QPixmap pixmap = this->map->renderBorder(true);
    for (auto item : this->borderItems) {
        item->setPixmap(pixmap);
    }
}

void Editor::updateMapConnections() {
    for (auto item : connection_items)
        item->render(true);
}

int Editor::getBorderDrawDistance(int dimension) {
    // Draw sufficient border blocks to fill the player's view (BORDER_DISTANCE)
    if (dimension >= BORDER_DISTANCE) {
        return dimension;
    } else if (dimension) {
        return dimension * (BORDER_DISTANCE / dimension + (BORDER_DISTANCE % dimension ? 1 : 0));
    } else {
        return BORDER_DISTANCE;
    }
}

void Editor::onToggleGridClicked(bool checked) {
    porymapConfig.showGrid = checked;
    if (ui->graphicsView_Map->scene())
        ui->graphicsView_Map->scene()->update();
}

void Editor::clearMapGrid() {
    for (QGraphicsLineItem* item : gridLines) {
        if (item) delete item;
    }
    gridLines.clear();
}

void Editor::displayMapGrid() {
    clearMapGrid();
    ui->checkBox_ToggleGrid->disconnect();

    int pixelWidth = map->getWidth() * 16;
    int pixelHeight = map->getHeight() * 16;
    for (int i = 0; i <= map->getWidth(); i++) {
        int x = i * 16;
        QGraphicsLineItem *line = new QGraphicsLineItem(x, 0, x, pixelHeight);
        line->setVisible(ui->checkBox_ToggleGrid->isChecked());
        gridLines.append(line);
        connect(ui->checkBox_ToggleGrid, &QCheckBox::toggled, [=](bool checked){line->setVisible(checked);});
    }
    for (int j = 0; j <= map->getHeight(); j++) {
        int y = j * 16;
        QGraphicsLineItem *line = new QGraphicsLineItem(0, y, pixelWidth, y);
        line->setVisible(ui->checkBox_ToggleGrid->isChecked());
        gridLines.append(line);
        connect(ui->checkBox_ToggleGrid, &QCheckBox::toggled, [=](bool checked){line->setVisible(checked);});
    }
    connect(ui->checkBox_ToggleGrid, &QCheckBox::toggled, this, &Editor::onToggleGridClicked);
}

void Editor::updatePrimaryTileset(QString tilesetLabel, bool forceLoad)
{
    if (map->layout->tileset_primary_label != tilesetLabel || forceLoad)
    {
        map->layout->tileset_primary_label = tilesetLabel;
        map->layout->tileset_primary = project->getTileset(tilesetLabel, forceLoad);
        map->clearBorderCache();
    }
}

void Editor::updateSecondaryTileset(QString tilesetLabel, bool forceLoad)
{
    if (map->layout->tileset_secondary_label != tilesetLabel || forceLoad)
    {
        map->layout->tileset_secondary_label = tilesetLabel;
        map->layout->tileset_secondary = project->getTileset(tilesetLabel, forceLoad);
        map->clearBorderCache();
    }
}

void Editor::toggleBorderVisibility(bool visible, bool enableScriptCallback)
{
    porymapConfig.showBorder = visible;
    updateBorderVisibility();
    if (enableScriptCallback)
        Scripting::cb_BorderVisibilityToggled(visible);
}

void Editor::updateBorderVisibility() {
    // On the connections tab the border is always visible, and the connections can be edited.
    bool editingConnections = (ui->mainTabBar->currentIndex() == MainTab::Connections);
    bool visible = (editingConnections || ui->checkBox_ToggleBorder->isChecked());

    // Update border
    const qreal borderOpacity = editingConnections ? 0.4 : 1;
    for (QGraphicsPixmapItem* item : borderItems) {
        item->setVisible(visible);
        item->setOpacity(borderOpacity);
    }

    // Update map connections
    for (ConnectionPixmapItem* item : connection_items) {
        item->setVisible(visible);
        item->setEditable(editingConnections);
        item->setEnabled(visible);

        // When connecting a map to itself we don't bother to re-render the map connections in real-time,
        // i.e. if the user paints a new metatile on the map this isn't immediately reflected in the connection.
        // We're rendering them now, so we take the opportunity to do a full re-render for self-connections.
        bool fullRender = (this->map && item->connection && this->map->name == item->connection->targetMapName());
        item->render(fullRender);
    }
}

void Editor::updateCustomMapHeaderValues(QTableWidget *table)
{
    map->customHeaders = CustomAttributesTable::getAttributes(table);
    map->modify();
}

Tileset* Editor::getCurrentMapPrimaryTileset()
{
    QString tilesetLabel = map->layout->tileset_primary_label;
    return project->getTileset(tilesetLabel);
}

QList<DraggablePixmapItem *> Editor::getEventPixmapItems() {
    QList<DraggablePixmapItem *> list;
    for (const auto &child : events_group->childItems()) {
        list.append(static_cast<DraggablePixmapItem *>(child));
    }
    return list;
}

void Editor::redrawEventPixmapItem(DraggablePixmapItem *item) {
    if (item && item->event && !item->event->getPixmap().isNull()) {
        qreal opacity = item->event->getUsingSprite() ? 1.0 : 0.7;
        item->setOpacity(opacity);
        project->setEventPixmap(item->event, true);
        item->setPixmap(item->event->getPixmap());
        item->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
        if (selected_events && selected_events->contains(item)) {
            QImage image = item->pixmap().toImage();
            QPainter painter(&image);
            painter.setPen(QColor(255, 0, 255));
            painter.drawRect(0, 0, image.width() - 1, image.height() - 1);
            painter.end();
            item->setPixmap(QPixmap::fromImage(image));
        }
        item->updatePosition();
    }
}

// Warp events display a warning if they're not positioned on a metatile with a warp behavior.
void Editor::updateWarpEventWarning(Event *event) {
    if (porymapConfig.warpBehaviorWarningDisabled)
        return;
    if (!project || !map || !event || event->getEventType() != Event::Type::Warp)
        return;
    Block block;
    Metatile * metatile = nullptr;
    WarpEvent * warpEvent = static_cast<WarpEvent*>(event);
    if (map->getBlock(warpEvent->getX(), warpEvent->getY(), &block)) {
        metatile = Tileset::getMetatile(block.metatileId(), map->layout->tileset_primary, map->layout->tileset_secondary);
    }
    // metatile may be null if the warp is in the map border. Display the warning in this case
    bool validWarpBehavior = metatile && projectConfig.warpBehaviors.contains(metatile->behavior());
    warpEvent->setWarningEnabled(!validWarpBehavior);
}

// The warp event behavior warning is updated whenever the event moves or the event selection changes.
// It does not respond to changes in the underlying metatile. To capture the common case of a user painting
// metatiles on the Map tab then returning to the Events tab we update the warnings for all selected warp
// events when the Events tab is opened. This does not cover the case where metatiles are painted while
// still on the Events tab, such as by Undo/Redo or the scripting API.
void Editor::updateWarpEventWarnings() {
    if (porymapConfig.warpBehaviorWarningDisabled)
        return;
    if (selected_events) {
        for (auto selection : *selected_events)
            updateWarpEventWarning(selection->event);
    }
}

void Editor::shouldReselectEvents() {
    selectNewEvents = true;
}

void Editor::updateSelectedEvents() {
    for (const auto &item : getEventPixmapItems()) {
        redrawEventPixmapItem(item);
    }
    emit updatedEvents();
}

void Editor::selectMapEvent(DraggablePixmapItem *item, bool toggle) {
    if (!selected_events || !item)
        return;

    if (!toggle) {
        // Selecting just this event
        selected_events->clear();
        selected_events->append(item);
    } else if (!selected_events->contains(item)) {
        // Adding event to group selection
        selected_events->append(item);
    } else if (selected_events->length() > 1) {
        // Removing event from group selection
        selected_events->removeOne(item);
    } else {
        // Attempting to toggle the only currently-selected event.
        // Unselecting an event this way would be unexpected, so we ignore it.
        return;
    }
    updateSelectedEvents();
}

void Editor::selectedEventIndexChanged(int index, Event::Group eventGroup) {
    int event_offs = Event::getIndexOffset(eventGroup);
    index = index - event_offs;
    Event *event = nullptr;
    if (index < this->map->events.value(eventGroup).length()) {
        event = this->map->events.value(eventGroup).at(index);
    }
    DraggablePixmapItem *selectedEvent = nullptr;
    for (QGraphicsItem *child : this->events_group->childItems()) {
        DraggablePixmapItem *item = static_cast<DraggablePixmapItem *>(child);
        if (item->event == event) {
            selectedEvent = item;
            break;
        }
    }

    if (selectedEvent) {
        this->selectMapEvent(selectedEvent);
    } else {
        updateSelectedEvents();
    }
}

void Editor::duplicateSelectedEvents() {
    if (!selected_events || !selected_events->length() || !map || !current_view || map_item->paintingMode != MapPixmapItem::PaintMode::Events)
        return;

    QList<Event *> selectedEvents;
    for (int i = 0; i < selected_events->length(); i++) {
        Event *original = selected_events->at(i)->event;
        Event::Type eventType = original->getEventType();
        if (eventLimitReached(eventType)) {
            logWarn(QString("Skipping duplication, the map limit for events of type '%1' has been reached.").arg(Event::typeToString(eventType)));
            continue;
        }
        if (eventType == Event::Type::HealLocation && !porymapConfig.allowHealLocationDeleting) {
            // Can't freely add Heal Locations if deleting them is not enabled.
            logWarn("Skipping duplication, adding Heal Locations is disabled.");
            continue;
        }
        Event *duplicate = original->duplicate();
        if (!duplicate) {
            logError("Encountered a problem duplicating an event.");
            continue;
        }
        duplicate->setX(duplicate->getX() + 1);
        duplicate->setY(duplicate->getY() + 1);
        selectedEvents.append(duplicate);
    }
    map->editHistory.push(new EventDuplicate(this, map, selectedEvents));
}

DraggablePixmapItem *Editor::addNewEvent(Event::Type type) {
    if (!project || !map || eventLimitReached(type))
        return nullptr;

    Event *event = Event::create(type);
    if (!event)
        return nullptr;

    event->setMap(this->map);
    event->setDefaultValues(this->project);

    map->editHistory.push(new EventCreate(this, map, event));
    return event->getPixmapItem();
}

// Currently only object events have an explicit limit
bool Editor::eventLimitReached(Event::Type event_type) {
    if (project && map) {
        if (Event::typeToGroup(event_type) == Event::Group::Object)
            return map->events.value(Event::Group::Object).length() >= project->getMaxObjectEvents();
    }
    return false;
}

void Editor::openMapScripts() const {
    openInTextEditor(map->getScriptsFilePath());
}

void Editor::openScript(const QString &scriptLabel) const {
    // Find the location of scriptLabel.
    QStringList scriptPaths(map->getScriptsFilePath());
    scriptPaths << project->getEventScriptsFilePaths();
    int lineNum = 0;
    QString scriptPath = scriptPaths.first();
    for (const auto &path : scriptPaths) {
        lineNum = ParseUtil::getScriptLineNumber(path, scriptLabel);
        if (lineNum != 0) {
            scriptPath = path;
            break;
        }
    }

    openInTextEditor(scriptPath, lineNum);
}

void Editor::openInTextEditor(const QString &path, int lineNum) {
    QString command = porymapConfig.textEditorGotoLine;
    if (command.isEmpty()) {
        // Open map scripts in the system's default editor.
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else {
        if (command.contains("%F")) {
            if (command.contains("%L"))
                command.replace("%L", QString::number(lineNum));
            command.replace("%F", '\"' + path + '\"');
        } else {
            command += " \"" + path + '\"';
        }
        Editor::startDetachedProcess(command);
    }
}

void Editor::openProjectInTextEditor() const {
    QString command = porymapConfig.textEditorOpenFolder;
    if (command.contains("%D"))
        command.replace("%D", '\"' + project->root + '\"');
    else
        command += " \"" + project->root + '\"';
    startDetachedProcess(command);
}

bool Editor::startDetachedProcess(const QString &command, const QString &workingDirectory, qint64 *pid) {
    logInfo("Executing command: " + command);
    QProcess process;
#ifdef Q_OS_WIN
    QStringList arguments = ParseUtil::splitShellCommand(command);
    const QString program = arguments.takeFirst();
    QFileInfo programFileInfo(program);
    if (programFileInfo.isExecutable()) {
        process.setProgram(program);
        process.setArguments(arguments);
    } else {
        // program is a batch script (such as VSCode's 'code' script) and needs to be started by cmd.exe.
        process.setProgram(QProcessEnvironment::systemEnvironment().value("COMSPEC"));
        // Windows is finicky with quotes on the command-line. I can't explain why this difference is necessary.
        if (command.startsWith('"'))
            process.setNativeArguments("/c \"" + command + '"');
        else
            process.setArguments(QStringList() << "/c" << program << arguments);
    }
#else
    QStringList arguments = ParseUtil::splitShellCommand(command);
    process.setProgram(arguments.takeFirst());
    process.setArguments(arguments);
#endif
    process.setWorkingDirectory(workingDirectory);
    return process.startDetached(pid);
}

// It doesn't seem to be possible to prevent the mousePress event
// from triggering both event's DraggablePixmapItem and the background mousePress.
// Since the DraggablePixmapItem's event fires first, we can set a temp
// variable "selectingEvent" so that we can detect whether or not the user
// is clicking on the background instead of an event.
void Editor::eventsView_onMousePress(QMouseEvent *event) {
    // make sure we are in event editing mode
    if (map_item && map_item->paintingMode != MapPixmapItem::PaintMode::Events) {
        return;
    }
    if (this->obj_edit_mode == "paint" && event->buttons() & Qt::RightButton) {
        this->obj_edit_mode = "select";
        this->settings->mapCursor = QCursor();
        this->cursorMapTileRect->setSingleTileMode();
        this->ui->toolButton_Paint->setChecked(false);
        this->ui->toolButton_Select->setChecked(true);
    }

    bool multiSelect = event->modifiers() & Qt::ControlModifier;
    if (!selectingEvent && !multiSelect && selected_events->length() > 1) {
        // User is clearing group selection by clicking on the background
        this->selectMapEvent(selected_events->first());
    }
    selectingEvent = false;
}

void Editor::setCollisionTabSpinBoxes(uint16_t collision, uint16_t elevation) {
    const QSignalBlocker blocker1(ui->spinBox_SelectedCollision);
    const QSignalBlocker blocker2(ui->spinBox_SelectedElevation);
    ui->spinBox_SelectedCollision->setValue(collision);
    ui->spinBox_SelectedElevation->setValue(elevation);
}

// Custom collision graphics may be provided by the user.
void Editor::setCollisionGraphics() {
    QString filepath = projectConfig.collisionSheetPath;

    QImage imgSheet;
    if (filepath.isEmpty()) {
        // No custom collision image specified, use the default.
        imgSheet = this->defaultCollisionImgSheet;
    } else {
        // Try to load custom collision image
        QString validPath = Project::getExistingFilepath(filepath);
        if (!validPath.isEmpty()) filepath = validPath; // Otherwise allow it to fail with the original path
        imgSheet = QImage(filepath);
        if (imgSheet.isNull()) {
            // Custom collision image failed to load, use default
            logWarn(QString("Failed to load custom collision image '%1', using default.").arg(filepath));
            imgSheet = this->defaultCollisionImgSheet;
        }
    }

    // Users are not required to provide an image that gives an icon for every elevation/collision combination.
    // Instead they tell us how many are provided in their image by specifying the number of columns and rows.
    const int imgColumns = projectConfig.collisionSheetWidth;
    const int imgRows = projectConfig.collisionSheetHeight;

    // Create a pixmap for the selector on the Collision tab. If a project was previously opened we'll also need to refresh the selector.
    this->collisionSheetPixmap = QPixmap::fromImage(imgSheet).scaled(MovementPermissionsSelector::CellWidth * imgColumns,
                                                                     MovementPermissionsSelector::CellHeight * imgRows);
    if (this->movement_permissions_selector_item)
        this->movement_permissions_selector_item->setBasePixmap(this->collisionSheetPixmap);

    for (auto sublist : collisionIcons)
        qDeleteAll(sublist);
    collisionIcons.clear();

    // Use the image sheet to create an icon for each collision/elevation combination.
    // Any icons for combinations that aren't provided by the image sheet are also created now using default graphics.
    const int w = 16, h = 16;
    imgSheet = imgSheet.scaled(w * imgColumns, h * imgRows);
    for (int collision = 0; collision <= Block::getMaxCollision(); collision++) {
        // If (collision >= imgColumns) here, it's a valid collision value, but it is not represented with an icon on the image sheet.
        // In this case we just use the rightmost collision icon. This is mostly to support the vanilla case, where technically 0-3
        // are valid collision values, but 1-3 have the same meaning, so the vanilla collision selector image only has 2 columns.
        int x = ((collision < imgColumns) ? collision : (imgColumns - 1)) * w;

        QList<const QImage*> sublist;
        for (int elevation = 0; elevation <= Block::getMaxElevation(); elevation++) {
            if (elevation < imgRows) {
                // This elevation has an icon on the image sheet, add it to the list
                int y = elevation * h;
                sublist.append(new QImage(imgSheet.copy(x, y, w, h)));
            } else {
                // This is a valid elevation value, but it has no icon on the image sheet.
                // Give it a placeholder "?" icon (red if impassable, white otherwise)
                sublist.append(new QImage(this->collisionPlaceholder.copy(x != 0 ? w : 0, 0, w, h)));
            }
        }
        collisionIcons.append(sublist);
    }
}
