#include "events.h"

#include "eventframes.h"
#include "project.h"
#include "config.h"

QMap<Event::Group, const QPixmap*> Event::icons;
QMap<Event::Type, QSet<QString>> Event::expectedFields;

Event* Event::create(Event::Type type) {
    switch (type) {
    case Event::Type::Object: return new ObjectEvent();
    case Event::Type::CloneObject: return new CloneObjectEvent();
    case Event::Type::Warp: return new WarpEvent();
    case Event::Type::Trigger: return new TriggerEvent();
    case Event::Type::WeatherTrigger: return new WeatherTriggerEvent();
    case Event::Type::Sign: return new SignEvent();
    case Event::Type::HiddenItem: return new HiddenItemEvent();
    case Event::Type::SecretBase: return new SecretBaseEvent();
    case Event::Type::HealLocation: return new HealLocationEvent();
    default: return nullptr;
    }
}

QString Event::groupToString(Event::Group group) {
    static const QMap<Event::Group, QString> eventGroupToString = {
        {Event::Group::Object, "Object"},
        {Event::Group::Warp,   "Warp"},
        {Event::Group::Coord,  "Trigger"},
        {Event::Group::Bg,     "BG"},
        {Event::Group::Heal,   "Heal Location"},
    };
    return eventGroupToString.value(group);
}

const QMap<Event::Type, QString> eventTypeToString = {
    {Event::Type::Object,          "Object"},
    {Event::Type::CloneObject,     "Clone Object"},
    {Event::Type::Warp,            "Warp"},
    {Event::Type::Trigger,         "Trigger"},
    {Event::Type::WeatherTrigger,  "Weather Trigger"},
    {Event::Type::Sign,            "Sign"},
    {Event::Type::HiddenItem,      "Hidden Item"},
    {Event::Type::SecretBase,      "Secret Base"},
    {Event::Type::HealLocation,    "Heal Location"},
};

QString Event::typeToString(Event::Type type) {
    return eventTypeToString.value(type);
}

Event::Type Event::typeFromString(QString type) {
    return eventTypeToString.key(type, Event::Type::None);
}

Event::Group Event::typeToGroup(Event::Type type) {
    static const QMap<Event::Type, Event::Group> eventTypeToGroup = {
        {Event::Type::Object,         Event::Group::Object},
        {Event::Type::CloneObject,    Event::Group::Object},
        {Event::Type::Warp,           Event::Group::Warp},
        {Event::Type::Trigger,        Event::Group::Coord},
        {Event::Type::WeatherTrigger, Event::Group::Coord},
        {Event::Type::Sign,           Event::Group::Bg},
        {Event::Type::HiddenItem,     Event::Group::Bg},
        {Event::Type::SecretBase,     Event::Group::Bg},
        {Event::Type::HealLocation,   Event::Group::Heal},
    };
    return eventTypeToGroup.value(type, Event::Group::None);
}

Event::~Event() {
    if (this->eventFrame)
        this->eventFrame->deleteLater();
}

EventFrame *Event::getEventFrame() {
    if (!this->eventFrame) createEventFrame();
    return this->eventFrame;
}

void Event::destroyEventFrame() {
    if (this->eventFrame) delete this->eventFrame;
    this->eventFrame = nullptr;
}

void Event::setPixmapItem(DraggablePixmapItem *item) {
    this->pixmapItem = item;
    if (this->eventFrame) {
        this->eventFrame->invalidateConnections();
    }
}

int Event::getEventIndex() const {
    return this->map ? this->map->events.value(this->getEventGroup()).indexOf(this) : -1;
}

void Event::setDefaultValues(Project *) {
    this->setX(0);
    this->setY(0);
    this->setElevation(projectConfig.defaultElevation);
    this->setDefaultCustomAttributes();
}

void Event::setDefaultCustomAttributes() {
    // TODO: Currently unsupported
    //this->setCustomAttributes(projectConfig.getDefaultEventCustomAttributes(this->getEventType()));
}

void Event::readCustomAttributes(const QJsonObject &json) {
    this->customAttributes.clear();
    for (auto i = json.constBegin(); i != json.constEnd(); i++) {
        if (i.key() == "x" || i.key() == "y") // All events are assumed to have an x and y field
            continue;
        if (Event::expectedFields[this->getEventType()].contains(i.key()))
            continue;
        this->customAttributes[i.key()] = i.value();
    }
}

void Event::addCustomAttributesTo(OrderedJson::object *obj) const {
    for (auto i = this->customAttributes.constBegin(); i != this->customAttributes.constEnd(); i++) {
        if (!obj->contains(i.key())) {
            (*obj)[i.key()] = OrderedJson::fromQJsonValue(i.value());
        }
    }
}

void Event::modify() {
    this->map->modify();
}

void Event::loadPixmap(Project *) {
    const QPixmap * pixmap = Event::icons.value(this->getEventGroup());
    this->pixmap = pixmap ? *pixmap : QPixmap();
}

void Event::clearIcons() {
    qDeleteAll(icons);
    icons.clear();
}

void Event::setIcons() {
    clearIcons();
    const int w = 16;
    const int h = 16;
    static const QPixmap defaultIcons = QPixmap(":/images/Entities_16x16.png");

    // Custom event icons may be provided by the user.
    const int numIcons = qMin(defaultIcons.width() / w, static_cast<int>(Event::Group::None));
    for (int i = 0; i < numIcons; i++) {
        Event::Group group = static_cast<Event::Group>(i);
        QString customIconPath = projectConfig.getEventIconPath(group);
        if (customIconPath.isEmpty()) {
            // No custom icon specified, use the default icon.
            icons[group] = new QPixmap(defaultIcons.copy(i * w, 0, w, h));
            continue;
        }

        // Try to load custom icon
        QString validPath = Project::getExistingFilepath(customIconPath);
        if (!validPath.isEmpty()) customIconPath = validPath; // Otherwise allow it to fail with the original path
        const QPixmap customIcon = QPixmap(customIconPath);
        if (customIcon.isNull()) {
            // Custom icon failed to load, use the default icon.
            icons[group] = new QPixmap(defaultIcons.copy(i * w, 0, w, h));
            logWarn(QString("Failed to load custom event icon '%1', using default icon.").arg(customIconPath));
        } else {
            icons[group] = new QPixmap(customIcon.scaled(w, h));
        }
    }
}

// Any field not listed here will be considered "custom" and appear in the table at the bottom of the event frame.
// Some of the fields may change depending on the user's project settings. Once the project is loaded they will remain the
// same for all events of that type, so to save time when loading events we only construct the field sets once per project load.
// TODO: Distribute to each class with a virtual function
void Event::initExpectedFields() {
    Event::expectedFields.clear();

    // Object
    QSet<QString> objectFields = {
        "graphics_id",
        "elevation",
        "movement_type",
        "movement_range_x",
        "movement_range_y",
        "trainer_type",
        "trainer_sight_or_berry_tree_id",
        "script",
        "flag",
    };
    if (projectConfig.eventCloneObjectEnabled) {
        objectFields.insert("type");
    }
    expectedFields.insert(Event::Type::Object, objectFields);

    // Clone Object
    static const QSet<QString> cloneObjectFields = {
        "type",
        "graphics_id",
        "target_local_id",
        "target_map",
    };
    expectedFields.insert(Event::Type::CloneObject, objectFields);

    // Warp
    static const QSet<QString> warpFields = {
        "elevation",
        "dest_map",
        "dest_warp_id",
    };
    expectedFields.insert(Event::Type::Warp, warpFields);

    // Trigger
    static const QSet<QString> triggerFields = {
        "type",
        "elevation",
        "var",
        "var_value",
        "script",
    };
    expectedFields.insert(Event::Type::Trigger, triggerFields);

    // Weather Trigger
    static const QSet<QString> weatherTriggerFields = {
        "type",
        "elevation",
        "weather",
    };
    expectedFields.insert(Event::Type::WeatherTrigger, weatherTriggerFields);

    // Sign
    static const QSet<QString> signFields = {
        "type",
        "elevation",
        "player_facing_dir",
        "script",
    };
    expectedFields.insert(Event::Type::Sign, signFields);

    // Hidden Item
    QSet<QString> hiddenItemFields = {
        "type",
        "elevation",
        "item",
        "flag",
    };
    if (projectConfig.hiddenItemQuantityEnabled) {
        hiddenItemFields.insert("quantity");
    }
    if (projectConfig.hiddenItemRequiresItemfinderEnabled) {
        hiddenItemFields.insert("underfoot");
    }
    expectedFields.insert(Event::Type::HiddenItem, hiddenItemFields);

    // Secret Base
    static const QSet<QString> secretBaseFields = {
        "type",
        "elevation",
        "secret_base_id",
    };
    expectedFields.insert(Event::Type::SecretBase, secretBaseFields);

    // Heal Location
    QSet<QString> healLocationFields = {
        "id",
    };
    if (projectConfig.healLocationRespawnDataEnabled) {
        healLocationFields.insert("respawn_map");
        healLocationFields.insert("respawn_npc");
    }
    expectedFields.insert(Event::Type::HealLocation, healLocationFields);
}


Event *ObjectEvent::duplicate() {
    ObjectEvent *copy = new ObjectEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setGfx(this->getGfx());
    copy->setMovement(this->getMovement());
    copy->setRadiusX(this->getRadiusX());
    copy->setRadiusY(this->getRadiusY());
    copy->setTrainerType(this->getTrainerType());
    copy->setSightRadiusBerryTreeID(this->getSightRadiusBerryTreeID());
    copy->setScript(this->getScript());
    copy->setFlag(this->getFlag());
    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *ObjectEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new ObjectFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object ObjectEvent::buildEventJson(Project *) const {
    OrderedJson::object objectJson;

    if (projectConfig.eventCloneObjectEnabled) {
        objectJson["type"] = "object";
    }
    objectJson["graphics_id"] = this->getGfx();
    objectJson["x"] = this->getX();
    objectJson["y"] = this->getY();
    objectJson["elevation"] = this->getElevation();
    objectJson["movement_type"] = this->getMovement();
    objectJson["movement_range_x"] = this->getRadiusX();
    objectJson["movement_range_y"] = this->getRadiusY();
    objectJson["trainer_type"] = this->getTrainerType();
    objectJson["trainer_sight_or_berry_tree_id"] = this->getSightRadiusBerryTreeID();
    objectJson["script"] = this->getScript();
    objectJson["flag"] = this->getFlag();
    this->addCustomAttributesTo(&objectJson);

    return objectJson;
}

bool ObjectEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setElevation(ParseUtil::jsonToInt(json["elevation"]));
    this->setGfx(ParseUtil::jsonToQString(json["graphics_id"]));
    this->setMovement(ParseUtil::jsonToQString(json["movement_type"]));
    this->setRadiusX(ParseUtil::jsonToInt(json["movement_range_x"]));
    this->setRadiusY(ParseUtil::jsonToInt(json["movement_range_y"]));
    this->setTrainerType(ParseUtil::jsonToQString(json["trainer_type"]));
    this->setSightRadiusBerryTreeID(ParseUtil::jsonToQString(json["trainer_sight_or_berry_tree_id"]));
    this->setScript(ParseUtil::jsonToQString(json["script"]));
    this->setFlag(ParseUtil::jsonToQString(json["flag"]));
    
    this->readCustomAttributes(json);

    return true;
}

void ObjectEvent::setDefaultValues(Project *project) {
    this->setGfx(project->gfxDefines.keys().value(0, "0"));
    this->setMovement(project->movementTypes.value(0, "0"));
    this->setScript("NULL");
    this->setTrainerType(project->trainerTypes.value(0, "0"));
    this->setFlag("0");
    this->setRadiusX(0);
    this->setRadiusY(0);
    this->setSightRadiusBerryTreeID("0");
    this->setFrameFromMovement(project->facingDirections.value(this->getMovement()));
    this->setDefaultCustomAttributes();
}

void ObjectEvent::loadPixmap(Project *project) {
    EventGraphics *eventGfx = project->eventGraphicsMap.value(this->gfx, nullptr);
    if (!eventGfx) {
        // Invalid gfx constant.
        // If this is a number, try to use that instead.
        bool ok;
        int altGfx = ParseUtil::gameStringToInt(this->gfx, &ok);
        if (ok && (altGfx < project->gfxDefines.count())) {
            eventGfx = project->eventGraphicsMap.value(project->gfxDefines.key(altGfx, "NULL"), nullptr);
        }
    }
    if (!eventGfx || eventGfx->spritesheet.isNull()) {
        // No sprite associated with this gfx constant.
        // Use default sprite instead.
        Event::loadPixmap(project);
        this->spriteWidth = 16;
        this->spriteHeight = 16;
        this->usingSprite = false;
    } else {
        this->setFrameFromMovement(project->facingDirections.value(this->movement));
        this->setPixmapFromSpritesheet(eventGfx);
    }
}

void ObjectEvent::setPixmapFromSpritesheet(EventGraphics * gfx)
{
    QImage img;
    if (gfx->inanimate) {
        img = gfx->spritesheet.copy(0, 0, gfx->spriteWidth, gfx->spriteHeight);
    } else {
        int x = 0;
        int y = 0;

        // Get frame's position in spritesheet.
        // Assume horizontal layout. If position would exceed sheet width, try vertical layout.
        if ((this->frame + 1) * gfx->spriteWidth <= gfx->spritesheet.width()) {
            x = this->frame * gfx->spriteWidth;
        } else if ((this->frame + 1) * gfx->spriteHeight <= gfx->spritesheet.height()) {
            y = this->frame * gfx->spriteHeight;
        }

        img = gfx->spritesheet.copy(x, y, gfx->spriteWidth, gfx->spriteHeight);

        // Right-facing sprite is just the left-facing sprite mirrored
        if (this->hFlip) {
            img = img.transformed(QTransform().scale(-1, 1));
        }
    }
    // Set first palette color fully transparent.
    img.setColor(0, qRgba(0, 0, 0, 0));
    pixmap = QPixmap::fromImage(img);
    this->spriteWidth = gfx->spriteWidth;
    this->spriteHeight = gfx->spriteHeight;
    this->usingSprite = true;
}

void ObjectEvent::setFrameFromMovement(QString facingDir) {
    // defaults
    // TODO: read this from a file somewhere?
    this->frame = 0;
    this->hFlip = false;
    if (facingDir == "DIR_NORTH") {
        this->frame = 1;
        this->hFlip = false;
    } else if (facingDir == "DIR_SOUTH") {
        this->frame = 0;
        this->hFlip = false;
    } else if (facingDir == "DIR_WEST") {
        this->frame = 2;
        this->hFlip = false;
    } else if (facingDir == "DIR_EAST") {
        this->frame = 2;
        this->hFlip = true;
    }
}



Event *CloneObjectEvent::duplicate() {
    CloneObjectEvent *copy = new CloneObjectEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setGfx(this->getGfx());
    copy->setTargetID(this->getTargetID());
    copy->setTargetMap(this->getTargetMap());
    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *CloneObjectEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new CloneObjectFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object CloneObjectEvent::buildEventJson(Project *project) const {
    OrderedJson::object cloneJson;

    cloneJson["type"] = "clone";
    cloneJson["graphics_id"] = this->getGfx();
    cloneJson["x"] = this->getX();
    cloneJson["y"] = this->getY();
    cloneJson["target_local_id"] = this->getTargetID();
    cloneJson["target_map"] = project->mapNameToMapConstant.value(this->getTargetMap());
    this->addCustomAttributesTo(&cloneJson);

    return cloneJson;
}

bool CloneObjectEvent::loadFromJson(QJsonObject json, Project *project) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setGfx(ParseUtil::jsonToQString(json["graphics_id"]));
    this->setTargetID(ParseUtil::jsonToInt(json["target_local_id"]));

    // Log a warning if "target_map" isn't a known map ID, but don't overwrite user data.
    const QString mapConstant = ParseUtil::jsonToQString(json["target_map"]);
    if (!project->mapConstantToMapName.contains(mapConstant))
        logWarn(QString("Target Map constant '%1' is invalid.").arg(mapConstant));
    this->setTargetMap(project->mapConstantToMapName.value(mapConstant, mapConstant));

    this->readCustomAttributes(json);

    return true;
}

void CloneObjectEvent::setDefaultValues(Project *project) {
    this->setGfx(project->gfxDefines.keys().value(0, "0"));
    this->setTargetID(1);
    if (this->getMap()) this->setTargetMap(this->getMap()->name);
    this->setDefaultCustomAttributes();
}

void CloneObjectEvent::loadPixmap(Project *project) {
    // Try to get the targeted object to clone
    int eventIndex = this->targetID - 1;
    Map *clonedMap = project->getMap(this->targetMap);
    Event *clonedEvent = clonedMap ? clonedMap->events[Event::Group::Object].value(eventIndex, nullptr) : nullptr;

    if (clonedEvent && clonedEvent->getEventType() == Event::Type::Object) {
        // Get graphics data from cloned object
        ObjectEvent *clonedObject = dynamic_cast<ObjectEvent *>(clonedEvent);
        this->gfx = clonedObject->getGfx();
        this->movement = clonedObject->getMovement();
    } else {
        // Invalid object specified, use default graphics data (as would be shown in-game)
        this->gfx = project->gfxDefines.key(0, "0");
        this->movement = project->movementTypes.value(0, "0");
    }

    EventGraphics *eventGfx = project->eventGraphicsMap.value(gfx, nullptr);
    if (!eventGfx || eventGfx->spritesheet.isNull()) {
        // No sprite associated with this gfx constant.
        // Use default sprite instead.
        Event::loadPixmap(project);
        this->spriteWidth = 16;
        this->spriteHeight = 16;
        this->usingSprite = false;
    } else {
        this->setFrameFromMovement(project->facingDirections.value(this->movement));
        this->setPixmapFromSpritesheet(eventGfx);
    }
}



Event *WarpEvent::duplicate() {
    WarpEvent *copy = new WarpEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setDestinationMap(this->getDestinationMap());
    copy->setDestinationWarpID(this->getDestinationWarpID());

    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *WarpEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new WarpFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object WarpEvent::buildEventJson(Project *project) const {
    OrderedJson::object warpJson;

    warpJson["x"] = this->getX();
    warpJson["y"] = this->getY();
    warpJson["elevation"] = this->getElevation();
    warpJson["dest_map"] = project->mapNameToMapConstant.value(this->getDestinationMap());
    warpJson["dest_warp_id"] = this->getDestinationWarpID();

    this->addCustomAttributesTo(&warpJson);

    return warpJson;
}

bool WarpEvent::loadFromJson(QJsonObject json, Project *project) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setElevation(ParseUtil::jsonToInt(json["elevation"]));
    this->setDestinationWarpID(ParseUtil::jsonToQString(json["dest_warp_id"]));

    // Log a warning if "dest_map" isn't a known map ID, but don't overwrite user data.
    const QString mapConstant = ParseUtil::jsonToQString(json["dest_map"]);
    if (!project->mapConstantToMapName.contains(mapConstant))
        logWarn(QString("Destination Map constant '%1' is invalid.").arg(mapConstant));
    this->setDestinationMap(project->mapConstantToMapName.value(mapConstant, mapConstant));

    this->readCustomAttributes(json);

    return true;
}

void WarpEvent::setDefaultValues(Project *) {
    if (this->getMap()) this->setDestinationMap(this->getMap()->name);
    this->setDestinationWarpID("0");
    this->setElevation(0);
    this->setDefaultCustomAttributes();
}

void WarpEvent::setWarningEnabled(bool enabled) {
    WarpFrame * frame = static_cast<WarpFrame*>(this->getEventFrame());
    if (frame && frame->warning)
        frame->warning->setVisible(enabled);
}



Event *TriggerEvent::duplicate() {
    TriggerEvent *copy = new TriggerEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setScriptVar(this->getScriptVar());
    copy->setScriptVarValue(this->getScriptVarValue());
    copy->setScriptLabel(this->getScriptLabel());

    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *TriggerEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new TriggerFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object TriggerEvent::buildEventJson(Project *) const {
    OrderedJson::object triggerJson;

    triggerJson["type"] = "trigger";
    triggerJson["x"] = this->getX();
    triggerJson["y"] = this->getY();
    triggerJson["elevation"] = this->getElevation();
    triggerJson["var"] = this->getScriptVar();
    triggerJson["var_value"] = this->getScriptVarValue();
    triggerJson["script"] = this->getScriptLabel();

    this->addCustomAttributesTo(&triggerJson);

    return triggerJson;
}

bool TriggerEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setElevation(ParseUtil::jsonToInt(json["elevation"]));
    this->setScriptVar(ParseUtil::jsonToQString(json["var"]));
    this->setScriptVarValue(ParseUtil::jsonToQString(json["var_value"]));
    this->setScriptLabel(ParseUtil::jsonToQString(json["script"]));

    this->readCustomAttributes(json);

    return true;
}

void TriggerEvent::setDefaultValues(Project *project) {
    this->setScriptLabel("NULL");
    this->setScriptVar(project->varNames.value(0, "0"));
    this->setScriptVarValue("0");
    this->setElevation(0);
    this->setDefaultCustomAttributes();
}



Event *WeatherTriggerEvent::duplicate() {
    WeatherTriggerEvent *copy = new WeatherTriggerEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setWeather(this->getWeather());

    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *WeatherTriggerEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new WeatherTriggerFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object WeatherTriggerEvent::buildEventJson(Project *) const {
    OrderedJson::object weatherJson;

    weatherJson["type"] = "weather";
    weatherJson["x"] = this->getX();
    weatherJson["y"] = this->getY();
    weatherJson["elevation"] = this->getElevation();
    weatherJson["weather"] = this->getWeather();

    this->addCustomAttributesTo(&weatherJson);

    return weatherJson;
}

bool WeatherTriggerEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setElevation(ParseUtil::jsonToInt(json["elevation"]));
    this->setWeather(ParseUtil::jsonToQString(json["weather"]));

    this->readCustomAttributes(json);

    return true;
}

void WeatherTriggerEvent::setDefaultValues(Project *project) {
    this->setWeather(project->coordEventWeatherNames.value(0, "0"));
    this->setElevation(0);
    this->setDefaultCustomAttributes();
}



Event *SignEvent::duplicate() {
    SignEvent *copy = new SignEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setFacingDirection(this->getFacingDirection());
    copy->setScriptLabel(this->getScriptLabel());

    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *SignEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new SignFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object SignEvent::buildEventJson(Project *) const {
    OrderedJson::object signJson;

    signJson["type"] = "sign";
    signJson["x"] = this->getX();
    signJson["y"] = this->getY();
    signJson["elevation"] = this->getElevation();
    signJson["player_facing_dir"] = this->getFacingDirection();
    signJson["script"] = this->getScriptLabel();

    this->addCustomAttributesTo(&signJson);

    return signJson;
}

bool SignEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setElevation(ParseUtil::jsonToInt(json["elevation"]));
    this->setFacingDirection(ParseUtil::jsonToQString(json["player_facing_dir"]));
    this->setScriptLabel(ParseUtil::jsonToQString(json["script"]));

    this->readCustomAttributes(json);

    return true;
}

void SignEvent::setDefaultValues(Project *project) {
    this->setFacingDirection(project->bgEventFacingDirections.value(0, "0"));
    this->setScriptLabel("NULL");
    this->setElevation(0);
    this->setDefaultCustomAttributes();
}



Event *HiddenItemEvent::duplicate() {
    HiddenItemEvent *copy = new HiddenItemEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setItem(this->getItem());
    copy->setFlag(this->getFlag());
    copy->setQuantity(this->getQuantity());
    copy->setQuantity(this->getQuantity());

    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *HiddenItemEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new HiddenItemFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object HiddenItemEvent::buildEventJson(Project *) const {
    OrderedJson::object hiddenItemJson;

    hiddenItemJson["type"] = "hidden_item";
    hiddenItemJson["x"] = this->getX();
    hiddenItemJson["y"] = this->getY();
    hiddenItemJson["elevation"] = this->getElevation();
    hiddenItemJson["item"] = this->getItem();
    hiddenItemJson["flag"] = this->getFlag();
    if (projectConfig.hiddenItemQuantityEnabled) {
        hiddenItemJson["quantity"] = this->getQuantity();
    }
    if (projectConfig.hiddenItemRequiresItemfinderEnabled) {
        hiddenItemJson["underfoot"] = this->getUnderfoot();
    }

    this->addCustomAttributesTo(&hiddenItemJson);

    return hiddenItemJson;
}

bool HiddenItemEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setElevation(ParseUtil::jsonToInt(json["elevation"]));
    this->setItem(ParseUtil::jsonToQString(json["item"]));
    this->setFlag(ParseUtil::jsonToQString(json["flag"]));
    if (projectConfig.hiddenItemQuantityEnabled) {
        this->setQuantity(ParseUtil::jsonToInt(json["quantity"]));
    }
    if (projectConfig.hiddenItemRequiresItemfinderEnabled) {
        this->setUnderfoot(ParseUtil::jsonToBool(json["underfoot"]));
    }

    this->readCustomAttributes(json);

    return true;
}

void HiddenItemEvent::setDefaultValues(Project *project) {
    this->setItem(project->itemNames.value(0, "0"));
    this->setFlag(project->flagNames.value(0, "0"));
    if (projectConfig.hiddenItemQuantityEnabled) {
        this->setQuantity(1);
    }
    if (projectConfig.hiddenItemRequiresItemfinderEnabled) {
        this->setUnderfoot(false);
    }
    this->setDefaultCustomAttributes();
}



Event *SecretBaseEvent::duplicate() {
    SecretBaseEvent *copy = new SecretBaseEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setBaseID(this->getBaseID());

    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *SecretBaseEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new SecretBaseFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object SecretBaseEvent::buildEventJson(Project *) const {
    OrderedJson::object secretBaseJson;

    secretBaseJson["type"] = "secret_base";
    secretBaseJson["x"] = this->getX();
    secretBaseJson["y"] = this->getY();
    secretBaseJson["elevation"] = this->getElevation();
    secretBaseJson["secret_base_id"] = this->getBaseID();

    this->addCustomAttributesTo(&secretBaseJson);

    return secretBaseJson;
}

bool SecretBaseEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setElevation(ParseUtil::jsonToInt(json["elevation"]));
    this->setBaseID(ParseUtil::jsonToQString(json["secret_base_id"]));

    this->readCustomAttributes(json);

    return true;
}

void SecretBaseEvent::setDefaultValues(Project *project) {
    this->setBaseID(project->secretBaseIds.value(0, "0"));
    this->setElevation(0);
    this->setDefaultCustomAttributes();
}



Event *HealLocationEvent::duplicate() {
    HealLocationEvent *copy = new HealLocationEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setIdName(this->getIdName());
    copy->setRespawnMapName(this->getRespawnMapName());
    copy->setRespawnNPC(this->getRespawnNPC());

    copy->setCustomAttributes(this->getCustomAttributes());

    return copy;
}

EventFrame *HealLocationEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new HealLocationFrame(this);
        this->eventFrame->setup();
    }
    return this->eventFrame;
}

OrderedJson::object HealLocationEvent::buildEventJson(Project *project) const {
    OrderedJson::object healLocationJson;

    healLocationJson["id"] = this->getIdName();
    healLocationJson["x"] = this->getX();
    healLocationJson["y"] = this->getY();
    if (projectConfig.healLocationRespawnDataEnabled) {
        const QString mapName = this->getRespawnMapName();
        healLocationJson["respawn_map"] = project->mapNameToMapConstant.value(mapName, mapName);
        healLocationJson["respawn_npc"] = this->getRespawnNPC();
    }

    this->addCustomAttributesTo(&healLocationJson);

    return healLocationJson;
}

bool HealLocationEvent::loadFromJson(QJsonObject json, Project *project) {
    this->setX(ParseUtil::jsonToInt(json["x"]));
    this->setY(ParseUtil::jsonToInt(json["y"]));
    this->setIdName(ParseUtil::jsonToQString(json["id"]));

    if (projectConfig.healLocationRespawnDataEnabled) {
        // Log a warning if "respawn_map" isn't a known map ID, but don't overwrite user data.
        const QString mapConstant = ParseUtil::jsonToQString(json["respawn_map"]);
        if (!project->mapConstantToMapName.contains(mapConstant))
            logWarn(QString("Respawn Map constant '%1' is invalid.").arg(mapConstant));
        this->setRespawnMapName(project->mapConstantToMapName.value(mapConstant, mapConstant));
        this->setRespawnNPC(ParseUtil::jsonToInt(json["respawn_npc"]));
    }

    this->readCustomAttributes(json);
    return true;
}

void HealLocationEvent::setDefaultValues(Project *project) {
    this->setIdName(this->map ? project->getDefaultHealLocationName(this->map->constantName) : QString());
    this->setRespawnMapName(this->map ? this->map->name : QString());
    this->setRespawnNPC(0 + this->getIndexOffset());
    this->setDefaultCustomAttributes();
}
