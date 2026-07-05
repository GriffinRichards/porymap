#include "eventframes.h"
#include "customattributesframe.h"
#include "editcommands.h"
#include "edithistoryspinbox.h"
#include "eventpixmapitem.h"
#include "flowlayout.h"

#include <QCompleter>
#include <QFileIconProvider>
#include <QLineEdit>
#include <QSpinBox>

#include <limits>
using std::numeric_limits;



static inline QSpacerItem *createSpacerH() {
    return new QSpacerItem(2, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
}


EventFrame::EventFrame(Event *event, QWidget *parent) : QFrame(parent), m_event(event) {
    // delete default layout due to lack of parent at initialization
    if (this->layout()) delete this->layout();

    auto mainLayout = new QVBoxLayout(this);

    setFrameStyle(QFrame::Box | QFrame::Raised);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    ///
    /// init widgets
    ///
    auto layout_xyz = new FlowLayout();

    // x spinner & label
    auto spinBox_x = new EditHistorySpinBox(this);
    spinBox_x->setMinimum(numeric_limits<int16_t>::min());
    spinBox_x->setMaximum(numeric_limits<int16_t>::max());
    spinBox_x->setValue(m_event->getX());
    connect(spinBox_x, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, spinBox_x](int value) {
        if (!m_event || !m_event->getMap()) return;
        int delta = value - m_event->getX();
        if (delta) {
            m_event->getMap()->commit(new EventMove(QList<Event *>() << m_event, delta, 0, spinBox_x->getActionId()));
        }
    });
    connect(m_event, &Event::xChanged, spinBox_x, &EditHistorySpinBox::setValue);

    auto widget_x = new QWidget();
    widget_x->setLayout(new QHBoxLayout());
    widget_x->layout()->setContentsMargins(0, 0, 8/*right*/, 0);
    widget_x->layout()->addWidget(new QLabel("x"));
    widget_x->layout()->addWidget(spinBox_x);
    layout_xyz->addWidget(widget_x);

    // y spinner & label
    auto spinBox_y = new EditHistorySpinBox(this);
    spinBox_y->setMinimum(numeric_limits<int16_t>::min());
    spinBox_y->setMaximum(numeric_limits<int16_t>::max());
    spinBox_y->setValue(m_event->getY());
    connect(spinBox_y, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, spinBox_y](int value) {
        if (!m_event || !m_event->getMap()) return;
        int delta = value - m_event->getY();
        if (delta) {
            m_event->getMap()->commit(new EventMove(QList<Event *>() << m_event, 0, delta, spinBox_y->getActionId()));
        }
    });
    connect(m_event, &Event::yChanged, spinBox_y, &EditHistorySpinBox::setValue);

    auto widget_y = new QWidget();
    widget_y->setLayout(new QHBoxLayout());
    widget_y->layout()->setContentsMargins(0, 0, 8/*right*/, 0);
    widget_y->layout()->addWidget(new QLabel("y"));
    widget_y->layout()->addWidget(spinBox_y);
    layout_xyz->addWidget(widget_y);

    // z spinner & label
    auto spinBox_z = new NoScrollSpinBox(this);
    spinBox_z->setMinimum(numeric_limits<int16_t>::min());
    spinBox_z->setMaximum(numeric_limits<int16_t>::max());
    spinBox_z->setValue(m_event->getZ());
    connect(spinBox_z, QOverload<int>::of(&QSpinBox::valueChanged), event, &Event::setZ);
    connect(event, &Event::zChanged, spinBox_z, &QSpinBox::setValue);

    m_widgetZ = new QWidget();
    m_widgetZ->setLayout(new QHBoxLayout());
    m_widgetZ->layout()->setContentsMargins(0, 0, 0, 0);
    m_widgetZ->layout()->addWidget(new QLabel("z"));
    m_widgetZ->layout()->addWidget(spinBox_z);
    layout_xyz->addWidget(m_widgetZ);

    layout_xyz->addItem(createSpacerH());

    auto l_vbox_1 = new QVBoxLayout();
    auto eventTypeLabel = new QLabel(Event::typeToString(m_event->getEventType()), this);
    l_vbox_1->addWidget(eventTypeLabel);
    l_vbox_1->addLayout(layout_xyz);

    // icon / pixmap label
    m_icon = new QLabel(this);
    m_icon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_icon->setMinimumSize(QSize(64, 64));
    m_icon->setScaledContents(false);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setFrameStyle(QFrame::Box | QFrame::Sunken);
    m_icon->setPixmap(m_event->getPixmap());

    auto l_hbox_1 = new QHBoxLayout();
    l_hbox_1->addWidget(m_icon);
    l_hbox_1->addLayout(l_vbox_1);
    mainLayout->addLayout(l_hbox_1);

    // for derived classes to add their own ui elements
    m_layoutContents = new QVBoxLayout();
    m_layoutContents->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(m_layoutContents);

    auto custom_attributes = new CustomAttributesFrame(this);
    custom_attributes->table()->setRestrictedKeys(m_event->getExpectedFields());
    custom_attributes->table()->setAttributes(m_event->getCustomAttributes());
    connect(custom_attributes->table(), &CustomAttributesTable::edited, this, [this, custom_attributes]() {
        if (!m_event) return;
        m_event->setCustomAttributes(custom_attributes->table()->getAttributes());
        m_event->modify();
    });

    // TODO: This needs to be forced to the bottom
    m_layoutContents->addWidget(custom_attributes);
}

void EventFrame::setElevationEnabled(bool enabled) {
    if (m_widgetZ) m_widgetZ->setVisible(enabled);
}

void EventFrame::setIcon(const QPixmap& pixmap) {
    if (m_icon) m_icon->setPixmap(pixmap);
}

void EventFrame::populate(Project *project) {
    m_populated = true;
    if (m_project && m_project != project) {
        // TODO: ? What are we disconnecting here, and why is it not being reconnected
        m_project->disconnect(this);
    }
    m_project = project;
}

void EventFrame::invalidateValues() {
    m_populated = false;
    if (this->isVisible()) {
        // Repopulate immediately
        this->populate(m_project);
    }
}
/*
void EventFrame::setActive(bool active) {
    this->setEnabled(active);
    this->blockSignals(!active);
}
*/

void EventFrame::populateDropdown(EventComboBox * combo, const QStringList &items) {
    // Set the items in the combo box. This may be called after the frame is initialized
    // if the frame needs to be repopulated, so ensure the text in the combo is preserved
    // and that we don't accidentally fire 'currentTextChanged'.
    const QSignalBlocker b(combo);
    const QString savedText = combo->currentText();
    combo->clear();
    combo->addItems(items);
    combo->setTextItem(savedText);
}

void EventFrame::populateScriptDropdown(EventComboBox * combo, Project * project) {
    // The script dropdown and autocomplete are populated with scripts used by the map's events and from its scripts file.
    Map *map = m_event ? m_event->getMap() : nullptr;
    if (!map)
        return;

    QStringList scripts = map->getScriptLabels(m_event->getEventGroup());
    populateDropdown(combo, scripts);

    // Depending on the settings, the autocomplete may also contain scripts from outside the map.
    if (project && porymapConfig.scriptAutocompleteMode != ScriptAutocompleteMode::MapOnly) {
        project->insertGlobalScriptLabels(scripts);
    }

    // Note: Because 'combo' is the parent, the old QCompleter will be deleted when a new one is set.
    auto completer = new QCompleter(scripts, combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    completer->setFilterMode(Qt::MatchContains);

    // Improve display speed for the autocomplete popup
    auto popup = (QListView *)completer->popup();
    if (popup) popup->setUniformItemSizes(true);

    combo->setCompleter(completer);

    // If the script labels change then we need to update the EventFrame.
    if (project) connect(project, &Project::eventScriptLabelsRead, this, &EventFrame::invalidateValues, Qt::UniqueConnection);
    connect(map, &Map::scriptsModified, this, &EventFrame::invalidateValues, Qt::UniqueConnection);
}

void EventFrame::populateMapNameDropdown(EventComboBox * combo, Project * project) {
    if (!project)
        return;

    populateDropdown(combo, project->mapNames());

    // This frame type displays map names, so when a new map is created we need to repopulate it.
    connect(project, &Project::mapCreated, this, &EventFrame::invalidateValues, Qt::UniqueConnection);
}

void EventFrame::populateIdNameDropdown(EventComboBox * combo, Project * project, const QString &mapName, Event::Group group) {
    if (!project || !project->isKnownMap(mapName))
        return;

    Map *map = project->loadMap(mapName);
    if (map) populateDropdown(combo, map->getEventIdNames(group));
}


ObjectFrame::ObjectFrame(ObjectEvent *object, QWidget *parent) : EventFrame(object, parent) {
    // local id
    auto l_form_local_id = new QFormLayout();
    auto lineEditLocalId = new QLineEdit(this);
    static const QString lineEditLocalId_ToolTip = Util::toHtmlParagraph("An optional, unique name to use to refer to this object in scripts. "
                                                                         "If no name is given you can refer to this object using its 'object id' number.");
    lineEditLocalId->setToolTip(lineEditLocalId_ToolTip);
    lineEditLocalId->setPlaceholderText("LOCALID_MY_NPC");
    lineEditLocalId->setText(object->getIdName());
    l_form_local_id->addRow("Local ID", lineEditLocalId);
    m_layoutContents->addLayout(l_form_local_id);
    connect(lineEditLocalId, &QLineEdit::textChanged, this, [this](const QString &text) {
        auto event = qobject_cast<ObjectEvent*>(m_event);
        if (!event) return;
        event->setIdName(text);
        event->modify();
    });

    // sprite combo
    QFormLayout *l_form_sprite = new QFormLayout();
    m_comboSprite = new EventComboBox(this);
    static const QString comboSprite_ToolTip = Util::toHtmlParagraph("The sprite graphics to use for this object.");
    m_comboSprite->setToolTip(comboSprite_ToolTip);
    m_comboSprite->setTextItem(object->getGfx());
    connect(m_comboSprite, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<ObjectEvent*>(m_event);
        if (!event) return;
        event->setGfx(text);
        event->getPixmapItem()->render(m_project);
        event->modify();
    });
    l_form_sprite->addRow("Sprite", m_comboSprite);
    m_layoutContents->addLayout(l_form_sprite);

    // movement
    QFormLayout *l_form_movement = new QFormLayout();
    m_comboMovement = new EventComboBox(this);
    static const QString comboMovement_ToolTip = Util::toHtmlParagraph("The object's natural movement behavior when the player is not interacting with it.");
    m_comboMovement->setToolTip(comboMovement_ToolTip);
    m_comboMovement->setTextItem(object->getMovement());
    l_form_movement->addRow("Movement", m_comboMovement);
    connect(m_comboMovement, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<ObjectEvent*>(m_event);
        if (!event) return;
        event->setMovement(text);
        event->getPixmapItem()->render(m_project);
        event->modify();
    });
    m_layoutContents->addLayout(l_form_movement);
    
    // movement radii
    QFormLayout *l_form_radii_xy = new QFormLayout();

    auto spinBox_RadiusX = new NoScrollSpinBox(this);
    spinBox_RadiusX->setMinimum(0);
    spinBox_RadiusX->setMaximum(255); // TODO: Replace with accessible value
    static const QString radiusX_ToolTip = Util::toHtmlParagraph("The maximum number of metatiles this object is allowed to move left "
                                                                 "or right during its normal movement behavior actions.");
    spinBox_RadiusX->setToolTip(radiusX_ToolTip);
    spinBox_RadiusX->setValue(object->getRadiusX());
    connect(spinBox_RadiusX, QOverload<int>::of(&QSpinBox::valueChanged), object, &ObjectEvent::setRadiusX);
    connect(object, &ObjectEvent::radiusXChanged, spinBox_RadiusX, &NoScrollSpinBox::setValue);

    auto spinBox_RadiusY = new NoScrollSpinBox(this);
    spinBox_RadiusY->setMinimum(0);
    spinBox_RadiusY->setMaximum(255);
    static const QString radiusY_ToolTip = Util::toHtmlParagraph("The maximum number of metatiles this object is allowed to move up "
                                                                 "or down during its normal movement behavior actions.");
    spinBox_RadiusY->setToolTip(radiusY_ToolTip);
    spinBox_RadiusY->setValue(object->getRadiusY());
    connect(spinBox_RadiusY, QOverload<int>::of(&QSpinBox::valueChanged), object, &ObjectEvent::setRadiusY);
    connect(object, &ObjectEvent::radiusYChanged, spinBox_RadiusY, &NoScrollSpinBox::setValue);

    l_form_radii_xy->addRow("Movement Radius X", spinBox_RadiusX);
    l_form_radii_xy->addRow("Movement Radius Y", spinBox_RadiusY);
    m_layoutContents->addLayout(l_form_radii_xy);

    // script
    QFormLayout *l_form_script = new QFormLayout();
    m_comboScript = new EventComboBox(this);
    static const QString script_ToolTip = Util::toHtmlParagraph("The script that is executed with this event.");
    m_comboScript->setToolTip(script_ToolTip);
    m_comboScript->setTextItem(object->getScript());
    connect(m_comboScript, &QComboBox::currentTextChanged, object, &ObjectEvent::setScript);
    connect(object, &ObjectEvent::scriptChanged, m_comboScript, &EventComboBox::setTextItem);

    // Add button next to combo which opens combo's current script.
    // TODO: This won't update until project reload..
    QToolButton* scriptButton{nullptr};
    if (!porymapConfig.textEditorGotoLine.isEmpty()) {
        scriptButton = new QToolButton(this);
        static const QString scriptButton_ToolTip = Util::toHtmlParagraph("Go to this script definition in text editor.");
        scriptButton->setToolTip(scriptButton_ToolTip);
        scriptButton->setFixedSize(m_comboScript->height(), m_comboScript->height());
        scriptButton->setIcon(QFileIconProvider().icon(QFileIconProvider::File));
        connect(scriptButton, &QToolButton::clicked, this, [this]() {
            if (!m_event || !m_event->getMap()) return;
            m_event->getMap()->openScript(m_comboScript->currentText());
        });
    }

    QHBoxLayout *l_hbox_scr = new QHBoxLayout();
    l_hbox_scr->setSpacing(3);
    l_hbox_scr->addWidget(m_comboScript);
    if (scriptButton) l_hbox_scr->addWidget(scriptButton);

    l_form_script->addRow("Script", l_hbox_scr);
    m_layoutContents->addLayout(l_form_script);

    // event flag
    QFormLayout *l_form_flag = new QFormLayout();
    m_comboFlag = new EventComboBox(this);
    static const QString flag_ToolTip = Util::toHtmlParagraph("The flag that hides the object when set.");
    m_comboFlag->setToolTip(flag_ToolTip);
    m_comboFlag->setTextItem(object->getFlag());
    connect(m_comboFlag, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<ObjectEvent*>(m_event);
        if (!event) return;
        event->setFlag(text);
        event->modify();
    });
    l_form_flag->addRow("Event Flag", m_comboFlag);
    m_layoutContents->addLayout(l_form_flag);

    // trainer type
    QFormLayout *l_form_trainer = new QFormLayout();
    m_comboTrainerType = new EventComboBox(this);
    static const QString trainerType_ToolTip = Util::toHtmlParagraph("The trainer type of this object event. If it is not a trainer, use NONE. "
                                                                     "SEE ALL DIRECTIONS should only be used with a sight radius of 1.");
    m_comboTrainerType->setToolTip(trainerType_ToolTip);
    m_comboTrainerType->setTextItem(object->getTrainerType());
    connect(m_comboTrainerType, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<ObjectEvent*>(m_event);
        if (!event) return;
        event->setTrainerType(text);
        event->modify();
    });
    l_form_trainer->addRow("Trainer Type", m_comboTrainerType);
    m_layoutContents->addLayout(l_form_trainer);

    // sight radius / berry tree id
    QFormLayout *l_form_radius_treeid = new QFormLayout();
    auto radiusTreeId_LineEdit = new QLineEdit(this);
    static const QString radiusTreeId_ToolTip = Util::toHtmlParagraph("The maximum sight range of a trainer, OR the unique id of the berry tree.");
    radiusTreeId_LineEdit->setToolTip(radiusTreeId_ToolTip);
    radiusTreeId_LineEdit->setText(object->getSightRadiusBerryTreeID());
    connect(radiusTreeId_LineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        auto event = qobject_cast<ObjectEvent*>(m_event);
        if (!event) return;
        event->setSightRadiusBerryTreeID(text);
        event->modify();
    });
    l_form_radius_treeid->addRow("Sight Radius / Berry Tree ID", radiusTreeId_LineEdit);
    m_layoutContents->addLayout(l_form_radius_treeid);

    // TODO: Switch to getting this signal from Event
    //connect(object->getPixmapItem(), &EventPixmapItem::rendered, this, &EventFrame::setIcon);
}

void ObjectFrame::populate(Project *project) {
    if (m_populated || !project) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    populateDropdown(m_comboSprite, project->gfxDefines.keys());
    populateDropdown(m_comboMovement, project->movementTypes);
    populateDropdown(m_comboFlag, project->flagNames);
    populateDropdown(m_comboTrainerType, project->trainerTypes);
    populateScriptDropdown(m_comboScript, project);
}



CloneObjectFrame::CloneObjectFrame(CloneObjectEvent *clone, QWidget *parent) : EventFrame(clone, parent) {
    setElevationEnabled(false);

    // local id
    QFormLayout *l_form_local_id = new QFormLayout();
    auto lineEditLocalId = new QLineEdit(this);
    static const QString lineEditLocalId_ToolTip = Util::toHtmlParagraph("An optional, unique name to use to refer to this object in scripts. "
                                                                         "If no name is given you can refer to this object using its 'object id' number.");
    lineEditLocalId->setToolTip(lineEditLocalId_ToolTip);
    lineEditLocalId->setPlaceholderText("LOCALID_MY_CLONE_NPC");
    lineEditLocalId->setText(clone->getIdName());
    connect(lineEditLocalId, &QLineEdit::textChanged, this, [this](const QString &text) {
        auto event = qobject_cast<CloneObjectEvent*>(m_event);
        if (!event) return;
        event->setIdName(text);
        event->modify();
    });
    l_form_local_id->addRow("Local ID", lineEditLocalId);
    m_layoutContents->addLayout(l_form_local_id);

    // sprite combo (edits disabled)
    QFormLayout *l_form_sprite = new QFormLayout();
    m_comboSprite = new EventComboBox(this);
    static const QString sprite_ToolTip = Util::toHtmlParagraph("The sprite graphics to use for this object. This is updated automatically "
                                                                "to match the target object, and so can't be edited. By default the games "
                                                                "will get the graphics directly from the target object, so this field is ignored.");
    m_comboSprite->setToolTip(sprite_ToolTip);
    m_comboSprite->setTextItem(clone->getGfx());
    l_form_sprite->addRow("Sprite", m_comboSprite);
    m_comboSprite->setEnabled(false);
    m_layoutContents->addLayout(l_form_sprite);

    // clone map id combo
    QFormLayout *l_form_dest_map = new QFormLayout();
    m_comboTargetMap = new EventComboBox(this);
    static const QString targetMap_ToolTip = Util::toHtmlParagraph("The name of the map that the object being cloned is on.");
    m_comboTargetMap->setToolTip(targetMap_ToolTip);
    m_comboTargetMap->setTextItem(clone->getTargetMap());
    connect(m_comboTargetMap, &QComboBox::currentTextChanged, this, [this](const QString &mapName) {
        auto event = qobject_cast<CloneObjectEvent*>(m_event);
        if (!event) return;
        event->setTargetMap(mapName);
        event->getPixmapItem()->render(m_project);
        m_comboSprite->setTextItem(event->getGfx());
        event->modify();
        populateIdNameDropdown(m_comboTargetId, m_project, mapName, Event::Group::Object);
    });
    l_form_dest_map->addRow("Target Map", m_comboTargetMap);
    m_layoutContents->addLayout(l_form_dest_map);

    // clone local id combo
    QFormLayout *l_form_dest_id = new QFormLayout();
    m_comboTargetId = new EventComboBox(this);
    static const QString targetId_ToolTip = Util::toHtmlParagraph("The Local ID name or number of the object being cloned.");
    m_comboTargetId->setToolTip(targetId_ToolTip);
    m_comboTargetId->setTextItem(clone->getTargetID());
    connect(m_comboTargetId, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<CloneObjectEvent*>(m_event);
        if (!event) return;
        event->setTargetID(text);
        event->getPixmapItem()->render(m_project);
        m_comboSprite->setTextItem(event->getGfx());
        event->modify();
    });
    l_form_dest_id->addRow("Target Local ID", m_comboTargetId);
    m_layoutContents->addLayout(l_form_dest_id);

    // TODO: Replace
    //connect(clone->getPixmapItem(), &EventPixmapItem::rendered, this, &EventFrame::setIcon);
    //connect(window, &MainWindow::mapOpened, this, &CloneObjectFrame::tryInvalidateIdDropdown, Qt::UniqueConnection);
}

void CloneObjectFrame::tryInvalidateIdDropdown(Map *map) {
    // If the clone's target map is opened then the names in this frame's ID dropdown may be changed.
    // Make sure we update the frame next time it's opened.
    auto event = qobject_cast<CloneObjectEvent*>(m_event);
    if (map && event && map->name() == event->getTargetMap()) {
        invalidateValues();
    }
}

void CloneObjectFrame::populate(Project *project) {
    if (m_populated) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    auto event = qobject_cast<CloneObjectEvent*>(m_event);
    populateMapNameDropdown(m_comboTargetMap, project);
    populateIdNameDropdown(m_comboTargetId, project, event->getTargetMap(), Event::Group::Object);
}


WarpFrame::WarpFrame(WarpEvent *warp, QWidget *parent) : EventFrame(warp, parent) {
    // ID
    QFormLayout *l_form_id = new QFormLayout();
    auto warpId_LineEdit = new QLineEdit(this);
    static const QString warpId_ToolTip = Util::toHtmlParagraph("An optional, unique name to use to refer to this warp from other warps. "
                                                                      "If no name is given you can refer to this warp using its 'warp id' number.");
    warpId_LineEdit->setToolTip(warpId_ToolTip);
    warpId_LineEdit->setPlaceholderText("WARP_ID_MY_WARP");
    warpId_LineEdit->setText(warp->getIdName());
    connect(warpId_LineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        auto event = qobject_cast<WarpEvent*>(m_event);
        if (!event) return;
        event->setIdName(text);
        event->modify();
    });
    l_form_id->addRow("ID", warpId_LineEdit);
    m_layoutContents->addLayout(l_form_id);

    // desination map combo
    QFormLayout *l_form_dest_map = new QFormLayout();
    m_comboDestMap = new EventComboBox(this);
    static const QString destMap_ToolTip = Util::toHtmlParagraph("The destination map name of the warp.");
    m_comboDestMap->setToolTip(destMap_ToolTip);
    m_comboDestMap->setTextItem(warp->getDestinationMap());
    connect(m_comboDestMap, &QComboBox::currentTextChanged, this, [this](const QString &mapName) {
        auto event = qobject_cast<WarpEvent*>(m_event);
        if (!event) return;
        event->setDestinationMap(mapName);
        event->modify();
        populateIdNameDropdown(m_comboDestWarp, m_project, mapName, Event::Group::Warp);
    });
    l_form_dest_map->addRow("Destination Map", m_comboDestMap);
    m_layoutContents->addLayout(l_form_dest_map);

    // desination warp id
    QFormLayout *l_form_dest_warp = new QFormLayout();
    m_comboDestWarp = new EventComboBox(this);
    static const QString destWarp_ToolTip = Util::toHtmlParagraph("The warp id on the destination map.");
    m_comboDestWarp->setToolTip(destWarp_ToolTip);
    m_comboDestWarp->setTextItem(warp->getDestinationWarpID());
    connect(m_comboDestWarp, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<WarpEvent*>(m_event);
        if (!event) return;
        event->setDestinationWarpID(text);
        event->modify();
    });
    l_form_dest_warp->addRow("Destination Warp", m_comboDestWarp);
    m_layoutContents->addLayout(l_form_dest_warp);

    // warning
    static const QString warningText = QStringLiteral("Warning:\nThis warp event is not positioned on a metatile with a warp behavior.\nClick this warning for more details.");
    QVBoxLayout *l_vbox_warning = new QVBoxLayout();
    m_warning = new QPushButton(warningText, this);
    m_warning->setFlat(true);
    m_warning->setStyleSheet("color: red; text-align: left");
    m_warning->setVisible(warp->getWarningEnabled());
    l_vbox_warning->addWidget(m_warning);
    m_layoutContents->addLayout(l_vbox_warning);
    // TODO: Connect warning to event signal

    // TODO: Replace
    //connect(window, &MainWindow::mapOpened, this, &WarpFrame::tryInvalidateIdDropdown, Qt::UniqueConnection);
    //connect(this->warning, &QPushButton::clicked, window, &MainWindow::onWarpBehaviorWarningClicked);
}

void WarpFrame::tryInvalidateIdDropdown(Map *map) {
    // If the warps's target map is opened then the names in this frame's ID dropdown may be changed.
    // Make sure we update the frame next time it's opened.
    auto event = qobject_cast<WarpEvent*>(m_event);
    if (map && event && map->name() == event->getDestinationMap()) {
        invalidateValues();
    }
}

void WarpFrame::populate(Project *project) {
    if (m_populated) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    auto event = qobject_cast<WarpEvent*>(m_event);
    populateMapNameDropdown(m_comboDestMap, project);
    if (event) populateIdNameDropdown(m_comboDestWarp, project, event->getDestinationMap(), Event::Group::Warp);
}



TriggerFrame::TriggerFrame(TriggerEvent *trigger, QWidget *parent) : EventFrame(trigger, parent) {
    // script combo
    QFormLayout *l_form_script = new QFormLayout();
    m_comboScript = new EventComboBox(this);
    static const QString script_ToolTip = Util::toHtmlParagraph("The script that is executed with this event.");
    m_comboScript->setToolTip(script_ToolTip);
    m_comboScript->setTextItem(trigger->getScriptLabel());
    connect(m_comboScript, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<TriggerEvent*>(m_event);
        if (!event) return;
        event->setScriptLabel(text);
        event->modify();
    });
    l_form_script->addRow("Script", m_comboScript);
    m_layoutContents->addLayout(l_form_script);

    // var combo
    QFormLayout *l_form_var = new QFormLayout();
    m_comboVar = new EventComboBox(this);
    static const QString var_ToolTip = Util::toHtmlParagraph("The variable by which the script is triggered. "
                                                                    "The script is triggered when this variable's value matches 'Var Value'.");
    m_comboVar->setToolTip(var_ToolTip);
    m_comboVar->setTextItem(trigger->getScriptVar());
    connect(m_comboVar, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<TriggerEvent*>(m_event);
        if (!event) return;
        event->setScriptVar(text);
        event->modify();
    });
    l_form_var->addRow("Var", m_comboVar);
    m_layoutContents->addLayout(l_form_var);

    // var value combo
    QFormLayout *l_form_var_val = new QFormLayout();
    auto varValue_LineEdit = new QLineEdit(this);
    static const QString varValue_ToolTip = Util::toHtmlParagraph("The variable's value that triggers the script.");
    varValue_LineEdit->setToolTip(varValue_ToolTip);
    varValue_LineEdit->setText(trigger->getScriptVarValue());
    connect(varValue_LineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        auto event = qobject_cast<TriggerEvent*>(m_event);
        if (!event) return;
        event->setScriptVarValue(text);
        event->modify();
    });
    l_form_var_val->addRow("Var Value", varValue_LineEdit);
    m_layoutContents->addLayout(l_form_var_val);
}

void TriggerFrame::populate(Project *project) {
    if (m_populated) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    populateDropdown(m_comboVar, project->varNames);
    populateScriptDropdown(m_comboScript, project);
}



WeatherTriggerFrame::WeatherTriggerFrame(WeatherTriggerEvent *weatherTrigger, QWidget *parent) : EventFrame(weatherTrigger, parent) {
    // weather combo
    QFormLayout *l_form_weather = new QFormLayout();
    m_comboWeather = new EventComboBox(this);
    static const QString weather_ToolTip = Util::toHtmlParagraph("The weather that starts when the player steps on this spot.");
    m_comboWeather->setToolTip(weather_ToolTip);
    m_comboWeather->setTextItem(weatherTrigger->getWeather());
    connect(m_comboWeather, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<WeatherTriggerEvent*>(m_event);
        if (!event) return;
        event->setWeather(text);
        event->modify();
    });
    l_form_weather->addRow("Weather", m_comboWeather);
    m_layoutContents->addLayout(l_form_weather);
}

void WeatherTriggerFrame::populate(Project *project) {
    if (m_populated || !project) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    populateDropdown(m_comboWeather, project->coordEventWeatherNames);
}



SignFrame::SignFrame(SignEvent *sign, QWidget *parent) : EventFrame(sign, parent) {
    // facing dir combo
    QFormLayout *l_form_facing_dir = new QFormLayout();
    m_comboFacingDir = new EventComboBox(this);
    static const QString facingDir_ToolTip = Util::toHtmlParagraph("The direction that the player must be facing to be able to interact with this event.");
    m_comboFacingDir->setToolTip(facingDir_ToolTip);
    m_comboFacingDir->setTextItem(sign->getFacingDirection());
    connect(m_comboFacingDir, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<SignEvent*>(m_event);
        if (!event) return;
        event->setFacingDirection(text);
        event->modify();
    });
    l_form_facing_dir->addRow("Player Facing Direction", m_comboFacingDir);
    m_layoutContents->addLayout(l_form_facing_dir);

    // script combo
    QFormLayout *l_form_script = new QFormLayout();
    m_comboScript = new EventComboBox(this);
    static const QString script_ToolTip = Util::toHtmlParagraph("The script that is executed with this event.");
    m_comboScript->setToolTip(script_ToolTip);
    m_comboScript->setTextItem(sign->getScriptLabel());
    connect(m_comboScript, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<SignEvent*>(m_event);
        if (!event) return;
        event->setScriptLabel(text);
        event->modify();
    });
    l_form_script->addRow("Script", m_comboScript);
    m_layoutContents->addLayout(l_form_script);
}

void SignFrame::populate(Project *project) {
    if (m_populated) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    populateDropdown(m_comboFacingDir, project->bgEventFacingDirections);
    populateScriptDropdown(m_comboScript, project);
}



HiddenItemFrame::HiddenItemFrame(HiddenItemEvent *hiddenItem, QWidget *parent) : EventFrame(hiddenItem, parent) {
    // item combo
    QFormLayout *l_form_item = new QFormLayout();
    m_comboItem = new EventComboBox(this);
    static const QString item_ToolTip = Util::toHtmlParagraph("The item to be given.");
    m_comboItem->setToolTip(item_ToolTip);
    m_comboItem->setTextItem(hiddenItem->getItem());
    connect(m_comboItem, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<HiddenItemEvent*>(m_event);
        if (!event) return;
        event->setItem(text);
        event->modify();
    });
    l_form_item->addRow("Item", m_comboItem);
    m_layoutContents->addLayout(l_form_item);

    // flag combo
    QFormLayout *l_form_flag = new QFormLayout();
    m_comboFlag = new EventComboBox(this);
    static const QString flag_ToolTip = Util::toHtmlParagraph("The flag that is set when the hidden item is picked up.");
    m_comboFlag->setToolTip(flag_ToolTip);
    m_comboFlag->setTextItem(hiddenItem->getFlag());
    connect(m_comboFlag, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<HiddenItemEvent*>(m_event);
        if (!event) return;
        event->setFlag(text);
        event->modify();
    });
    l_form_flag->addRow("Flag", m_comboFlag);
    m_layoutContents->addLayout(l_form_flag);

    // quantity spinner
    m_quantityFrame = new QFrame();
    QFormLayout *l_form_quantity = new QFormLayout(m_quantityFrame);
    l_form_quantity->setContentsMargins(0, 0, 0, 0);
    auto spinBox_Quantity = new NoScrollSpinBox(m_quantityFrame);
    static const QString quantity_ToolTip = Util::toHtmlParagraph("The number of items received when the hidden item is picked up.");
    spinBox_Quantity->setToolTip(quantity_ToolTip);
    spinBox_Quantity->setMinimum(1);
    spinBox_Quantity->setMaximum(255);
    spinBox_Quantity->setValue(hiddenItem->getQuantity());
    connect(spinBox_Quantity, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        auto event = qobject_cast<HiddenItemEvent*>(m_event);
        if (!event) return;
        event->setQuantity(value);
        event->modify();
    });
    l_form_quantity->addRow("Quantity", spinBox_Quantity);
    m_layoutContents->addWidget(m_quantityFrame);

    // itemfinder checkbox
    m_requiresItemfinderFrame = new QFrame();
    QFormLayout *l_form_itemfinder = new QFormLayout(m_requiresItemfinderFrame);
    l_form_itemfinder->setContentsMargins(0, 0, 0, 0);
    auto checkBox_RequiresItemfinder = new QCheckBox(m_requiresItemfinderFrame);
    static const QString itemfinder_ToolTip = Util::toHtmlParagraph("If checked, hidden item can only be picked up using the Itemfinder");
    checkBox_RequiresItemfinder->setToolTip(itemfinder_ToolTip);
    checkBox_RequiresItemfinder->setChecked(hiddenItem->getUnderfoot());
    connect(checkBox_RequiresItemfinder, &QCheckBox::toggled, this, [this](bool checked) {
        auto event = qobject_cast<HiddenItemEvent*>(m_event);
        if (!event) return;
        event->setUnderfoot(checked);
        event->modify();
    });
    l_form_itemfinder->addRow("Requires Itemfinder", checkBox_RequiresItemfinder);
    m_layoutContents->addWidget(m_requiresItemfinderFrame);
}

void HiddenItemFrame::populate(Project *project) {
    if (m_populated) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    m_quantityFrame->setVisible(projectConfig.hiddenItemQuantityEnabled);
    m_requiresItemfinderFrame->setVisible(projectConfig.hiddenItemRequiresItemfinderEnabled);

    populateDropdown(m_comboItem, project->itemNames);
    populateDropdown(m_comboFlag, project->flagNames);
}



SecretBaseFrame::SecretBaseFrame(SecretBaseEvent *secretBase, QWidget *parent) : EventFrame(secretBase, parent) {
    // item combo
    QFormLayout *l_form_base_id = new QFormLayout();
    m_comboBaseId = new EventComboBox(this);
    static const QString baseId_ToolTip = Util::toHtmlParagraph("The secret base id that is inside this secret base entrance. "
                                                                       "Secret base ids are meant to be unique to each and every secret base entrance.");
    m_comboBaseId->setToolTip(baseId_ToolTip);
    m_comboBaseId->setTextItem(secretBase->getBaseID());
    connect(m_comboBaseId, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<SecretBaseEvent*>(m_event);
        if (!event) return;
        event->setBaseID(text);
        event->modify();
    });
    l_form_base_id->addRow("Secret Base", m_comboBaseId);
    m_layoutContents->addLayout(l_form_base_id);
}

void SecretBaseFrame::populate(Project *project) {
    if (m_populated) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    populateDropdown(m_comboBaseId, project->secretBaseIds);
}



HealLocationFrame::HealLocationFrame(HealLocationEvent *healLocation, QWidget *parent) : EventFrame(healLocation, parent) {
    this->setElevationEnabled(false);

    // ID
    QFormLayout *l_form_id = new QFormLayout();
    auto id_LineEdit = new QLineEdit(this);
    static const QString id_ToolTip = Util::toHtmlParagraph("The unique identifier for this heal location.");
    id_LineEdit->setToolTip(id_ToolTip);
    id_LineEdit->setPlaceholderText(projectConfig.getIdentifier(ProjectIdentifier::define_heal_locations_prefix) + "MY_MAP");
    id_LineEdit->setText(healLocation->getIdName());
    connect(id_LineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        auto event = qobject_cast<HealLocationEvent*>(m_event);
        if (!event) return;
        event->setIdName(text);
        event->modify();
    });
    l_form_id->addRow("ID", id_LineEdit);
    m_layoutContents->addLayout(l_form_id);

    // respawn map combo
    m_respawnMapFrame = new QFrame();
    QFormLayout *l_form_respawn_map = new QFormLayout(m_respawnMapFrame);
    l_form_respawn_map->setContentsMargins(0, 0, 0, 0);
    m_comboRespawnMap = new EventComboBox(m_respawnMapFrame);
    static const QString respawnMap_ToolTip = Util::toHtmlParagraph("The map where the player will respawn after whiteout.");
    m_comboRespawnMap->setToolTip(respawnMap_ToolTip);
    m_comboRespawnMap->setTextItem(healLocation->getRespawnMapName());
    connect(m_comboRespawnMap, &QComboBox::currentTextChanged, this, [this](const QString &mapName) {
        auto event = qobject_cast<HealLocationEvent*>(m_event);
        if (!event) return;
        event->setRespawnMapName(mapName);
        event->modify();
        populateIdNameDropdown(m_comboRespawnNPC, m_project, mapName, Event::Group::Object);
    });
    l_form_respawn_map->addRow("Respawn Map", m_comboRespawnMap);
    m_layoutContents->addWidget(m_respawnMapFrame);

    // npc spinner
    m_respawnNPCFrame = new QFrame();
    QFormLayout *l_form_respawn_npc = new QFormLayout(m_respawnNPCFrame);
    l_form_respawn_npc->setContentsMargins(0, 0, 0, 0);
    m_comboRespawnNPC = new EventComboBox(m_respawnNPCFrame);
    static const QString respawnNPC_ToolTip = Util::toHtmlParagraph("The Local ID name or number of the NPC the player "
                                                                           "interacts with upon respawning after whiteout.");
    m_comboRespawnNPC->setToolTip(respawnNPC_ToolTip);
    m_comboRespawnNPC->setTextItem(healLocation->getRespawnNPC());
    connect(m_comboRespawnNPC, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        auto event = qobject_cast<HealLocationEvent*>(m_event);
        if (!event) return;
        event->setRespawnNPC(text);
        event->modify();
    });
    l_form_respawn_npc->addRow("Respawn NPC", m_comboRespawnNPC);
    m_layoutContents->addWidget(m_respawnNPCFrame);

    // TODO:
    //connect(window, &MainWindow::mapOpened, this, &HealLocationFrame::tryInvalidateIdDropdown, Qt::UniqueConnection);
}

void HealLocationFrame::tryInvalidateIdDropdown(Map *map) {
    // If the heal locations's target map is opened then the names in this frame's ID dropdown may be changed.
    // Make sure we update the frame next time it's opened.
    auto event = qobject_cast<HealLocationEvent*>(m_event);
    if (map && event && map->name() == event->getRespawnMapName()) {
        invalidateValues();
    }
}

void HealLocationFrame::populate(Project *project) {
    if (m_populated) return;

    const QSignalBlocker blocker(this);
    EventFrame::populate(project);

    bool respawnEnabled = projectConfig.healLocationRespawnDataEnabled;
    m_respawnMapFrame->setVisible(respawnEnabled);
    m_respawnNPCFrame->setVisible(respawnEnabled);
    if (respawnEnabled) {
        auto event = qobject_cast<HealLocationEvent*>(m_event);
        populateMapNameDropdown(m_comboRespawnMap, project);
        if (event) populateIdNameDropdown(m_comboRespawnNPC, project, event->getRespawnMapName(), Event::Group::Object);
    }
}
