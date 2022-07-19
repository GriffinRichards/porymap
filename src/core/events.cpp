#include "events.h"

#include "eventframes.h"
#include "project.h"
#include "config.h"



Event::~Event() {
    if (this->eventFrame)
        this->eventFrame->deleteLater();
}

EventFrame *Event::getEventFrame() {
    if (!this->eventFrame) createEventFrame();
    return this->eventFrame;
}

void Event::destroyEventFrame() {
    if (eventFrame) delete eventFrame;
    eventFrame = nullptr;
}

int Event::getEventIndex() {
    return this->map->events.value(this->getEventGroup()).indexOf(this);
}

void Event::setDefaultValues(Project *) {
    this->setX(0);
    this->setY(0);
    this->setElevation(3);
}

void Event::readCustomValues(QJsonObject values) {
    this->customValues.clear();
    QSet<QString> expectedFields = this->getExpectedFields();
    for (QString key : values.keys()) {
        if (!expectedFields.contains(key)) {
            this->customValues[key] = values[key].toString();
        }
    }
}

void Event::addCustomValuesTo(OrderedJson::object *obj) {
    for (QString key : this->customValues.keys()) {
        if (!obj->contains(key)) {
            (*obj)[key] = this->customValues[key];
        }
    }
}

QString Event::eventTypeToString(Event::Type type) {
    switch (type) {
    case Event::Type::Object:
        return "event_object";
    case Event::Type::CloneObject:
        return "event_clone_object";
    case Event::Type::Warp:
        return "event_warp";
    case Event::Type::Trigger:
        return "event_trigger";
    case Event::Type::WeatherTrigger:
        return "event_weather_trigger";
    case Event::Type::Sign:
        return "event_sign";
    case Event::Type::HiddenItem:
        return "event_hidden_item";
    case Event::Type::SecretBase:
        return "event_secret_base";
    case Event::Type::HealLocation:
        return "event_healspot";
    default:
        return "";
    }
}

Event::Type Event::eventTypeFromString(QString type) {
    if (type == "event_object") {
        return Event::Type::Object;
    } else if (type == "event_clone_object") {
        return Event::Type::CloneObject;
    } else if (type == "event_warp") {
        return Event::Type::Warp;
    } else if (type == "event_trigger") {
        return Event::Type::Trigger;
    } else if (type == "event_weather_trigger") {
        return Event::Type::WeatherTrigger;
    } else if (type == "event_sign") {
        return Event::Type::Sign;
    } else if (type == "event_hidden_item") {
        return Event::Type::HiddenItem;
    } else if (type == "event_secret_base") {
        return Event::Type::SecretBase;
    } else if (type == "event_healspot") {
        return Event::Type::HealLocation;
    } else {
        return Event::Type::None;
    }
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
    copy->setCustomValues(this->getCustomValues());

    return copy;
}

EventFrame *ObjectEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new ObjectFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object ObjectEvent::buildEventJson(Project *) {
    OrderedJson::object objectJson;

    if (projectConfig.getEventCloneObjectEnabled()) {
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
    this->addCustomValuesTo(&objectJson);

    return objectJson;
}

bool ObjectEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(json["x"].toInt());
    this->setY(json["y"].toInt());
    this->setElevation(json["elevation"].toInt());
    this->setGfx(json["graphics_id"].toString());
    this->setMovement(json["movement_type"].toString());
    this->setRadiusX(json["movement_range_x"].toInt());
    this->setRadiusY(json["movement_range_y"].toInt());
    this->setTrainerType(json["trainer_type"].toString());
    this->setSightRadiusBerryTreeID(json["trainer_sight_or_berry_tree_id"].toString());
    this->setScript(json["script"].toString());
    this->setFlag(json["flag"].toString());
    
    this->readCustomValues(json);

    return true;
}

void ObjectEvent::setDefaultValues(Project *project) {
    this->setGfx(project->gfxDefines.keys().first());
    this->setMovement(project->movementTypes.first());
    this->setScript("NULL");
    this->setTrainerType(project->trainerTypes.value(0, "0"));

    this->setRadiusX(0);
    this->setRadiusY(0);

    this->setFrameFromMovement(project->facingDirections.value(this->getMovement()));
}

const QSet<QString> expectedObjectFields = {
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

QSet<QString> ObjectEvent::getExpectedFields() {
    QSet<QString> expectedFields = QSet<QString>();
    expectedFields = expectedObjectFields;
    if (projectConfig.getEventCloneObjectEnabled()) {
        expectedFields.insert("type");
    }
    expectedFields << "x" << "y";
    return expectedFields;
}

void ObjectEvent::loadPixmap(Project *project) {
    EventGraphics *eventGfx = project->eventGraphicsMap.value(gfx, nullptr);
    if (!eventGfx || eventGfx->spritesheet.isNull()) {
        // No sprite associated with this gfx constant.
        // Use default sprite instead.
        this->pixmap = project->entitiesPixmap.copy(0, 0, 16, 16);
    } else {
        this->setFrameFromMovement(project->facingDirections.value(this->movement));
        this->setPixmapFromSpritesheet(eventGfx->spritesheet, eventGfx->spriteWidth, eventGfx->spriteHeight, eventGfx->inanimate);
    }
}

void ObjectEvent::setPixmapFromSpritesheet(QImage spritesheet, int spriteWidth, int spriteHeight, bool inanimate)
{
    int frame = inanimate ? 0 : this->frame;
    QImage img = spritesheet.copy(frame * spriteWidth % spritesheet.width(), 0, spriteWidth, spriteHeight);
    if (this->hFlip && !inanimate) {
        img = img.transformed(QTransform().scale(-1, 1));
    }
    // Set first palette color fully transparent.
    img.setColor(0, qRgba(0, 0, 0, 0));
    pixmap = QPixmap::fromImage(img);
    this->spriteWidth = spriteWidth;
    this->spriteHeight = spriteHeight;
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
    copy->setCustomValues(this->getCustomValues());

    return copy;
}

EventFrame *CloneObjectEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new CloneObjectFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object CloneObjectEvent::buildEventJson(Project *project) {
    OrderedJson::object cloneJson;

    cloneJson["type"] = "clone";
    cloneJson["graphics_id"] = this->getGfx();
    cloneJson["x"] = this->getX();
    cloneJson["y"] = this->getY();
    cloneJson["target_local_id"] = this->getTargetID();
    cloneJson["target_map"] = project->mapNamesToMapConstants.value(this->getTargetMap());
    this->addCustomValuesTo(&cloneJson);

    return cloneJson;
}

bool CloneObjectEvent::loadFromJson(QJsonObject json, Project *project) {
    this->setX(json["x"].toInt());
    this->setY(json["y"].toInt());
    this->setGfx(json["graphics_id"].toString());
    this->setTargetID(json["target_local_id"].toInt());

    // Ensure the target map constant is valid before adding it to the events.
    QString mapConstant = json["target_map"].toString();
    if (project->mapConstantsToMapNames.contains(mapConstant)) {
        this->setTargetMap(project->mapConstantsToMapNames.value(mapConstant));
    } else if (mapConstant == NONE_MAP_CONSTANT) {
        this->setTargetMap(NONE_MAP_NAME);
    } else {
        logError(QString("Destination map constant '%1' is invalid").arg(mapConstant));
        return false;
    }

    this->readCustomValues(json);

    return true;
}

void CloneObjectEvent::setDefaultValues(Project *project) {
    this->setGfx(project->gfxDefines.keys().first());
    this->setTargetID(1);
    if (this->getMap()) this->setTargetMap(this->getMap()->name);
}

const QSet<QString> expectedCloneObjectFields = {
    "type",
    "graphics_id",
    "target_local_id",
    "target_map",
};

QSet<QString> CloneObjectEvent::getExpectedFields() {
    QSet<QString> expectedFields = QSet<QString>();
    expectedFields = expectedCloneObjectFields;
    expectedFields << "x" << "y";
    return expectedFields;
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
        this->gfx = project->gfxDefines.key(0);
        this->movement = project->movementTypes.first();
    }

    EventGraphics *eventGfx = project->eventGraphicsMap.value(gfx, nullptr);
    if (!eventGfx || eventGfx->spritesheet.isNull()) {
        // No sprite associated with this gfx constant.
        // Use default sprite instead.
        this->pixmap = project->entitiesPixmap.copy(0, 0, 16, 16);
    } else {
        this->setFrameFromMovement(project->facingDirections.value(this->movement));
        this->setPixmapFromSpritesheet(eventGfx->spritesheet, eventGfx->spriteWidth, eventGfx->spriteHeight, eventGfx->inanimate);
    }
}



Event *WarpEvent::duplicate() {
    WarpEvent *copy = new WarpEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setDestinationMap(this->getDestinationMap());
    copy->setDestinationWarpID(this->getDestinationWarpID());

    copy->setCustomValues(this->getCustomValues());

    return copy;
}

EventFrame *WarpEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new WarpFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object WarpEvent::buildEventJson(Project *project) {
    OrderedJson::object warpJson;

    warpJson["x"] = this->getX();
    warpJson["y"] = this->getY();
    warpJson["elevation"] = this->getElevation();
    warpJson["dest_map"] = project->mapNamesToMapConstants.value(this->getDestinationMap());
    warpJson["dest_warp_id"] = this->getDestinationWarpID();

    this->addCustomValuesTo(&warpJson);

    return warpJson;
}

bool WarpEvent::loadFromJson(QJsonObject json, Project *project) {
    this->setX(json["x"].toInt());
    this->setY(json["y"].toInt());
    this->setElevation(json["elevation"].toInt());
    this->setDestinationWarpID(json["dest_warp_id"].toInt());

    // Ensure the warp destination map constant is valid before adding it to the warps.
    QString mapConstant = json["dest_map"].toString();
    if (project->mapConstantsToMapNames.contains(mapConstant)) {
        this->setDestinationMap(project->mapConstantsToMapNames.value(mapConstant));
    } else if (mapConstant == NONE_MAP_CONSTANT) {
        this->setDestinationMap(NONE_MAP_NAME);
    } else {
        logError(QString("Destination map constant '%1' is invalid for warp").arg(mapConstant));
        return false;
    }

    this->readCustomValues(json);

    return true;
}

void WarpEvent::setDefaultValues(Project *) {
    if (this->getMap()) this->setDestinationMap(this->getMap()->name);
    this->setDestinationWarpID(0);
    this->setElevation(0);
}

const QSet<QString> expectedWarpFields = {
    "elevation",
    "dest_map",
    "dest_warp_id",
};

QSet<QString> WarpEvent::getExpectedFields() {
    QSet<QString> expectedFields = QSet<QString>();
    expectedFields = expectedWarpFields;
    expectedFields << "x" << "y";
    return expectedFields;
}

void WarpEvent::loadPixmap(Project *project) {
    this->pixmap = project->entitiesPixmap.copy(16, 0, 16, 16);
}



void CoordEvent::loadPixmap(Project *project) {
    this->pixmap = project->entitiesPixmap.copy(32, 0, 16, 16);
}



Event *TriggerEvent::duplicate() {
    TriggerEvent *copy = new TriggerEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setScriptVar(this->getScriptVar());
    copy->setScriptVarValue(this->getScriptVarValue());
    copy->setScriptLabel(this->getScriptLabel());

    copy->setCustomValues(this->getCustomValues());

    return copy;
}

EventFrame *TriggerEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new TriggerFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object TriggerEvent::buildEventJson(Project *) {
    OrderedJson::object triggerJson;

    triggerJson["type"] = "trigger";
    triggerJson["x"] = this->getX();
    triggerJson["y"] = this->getY();
    triggerJson["elevation"] = this->getElevation();
    triggerJson["var"] = this->getScriptVar();
    triggerJson["var_value"] = this->getScriptVarValue();
    triggerJson["script"] = this->getScriptLabel();

    this->addCustomValuesTo(&triggerJson);

    return triggerJson;
}

bool TriggerEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(json["x"].toInt());
    this->setY(json["y"].toInt());
    this->setElevation(json["elevation"].toInt());
    this->setScriptVar(json["var"].toString());
    this->setScriptVarValue(json["var_value"].toString());
    this->setScriptLabel(json["script"].toString());

    this->readCustomValues(json);

    return true;
}

void TriggerEvent::setDefaultValues(Project *project) {
    this->setScriptLabel("NULL");
    this->setScriptVar(project->varNames.first());
    this->setScriptVarValue("0");
    this->setElevation(0);
}

const QSet<QString> expectedTriggerFields = {
    "type",
    "elevation",
    "var",
    "var_value",
    "script",
};

QSet<QString> TriggerEvent::getExpectedFields() {
    QSet<QString> expectedFields = QSet<QString>();
    expectedFields = expectedTriggerFields;
    expectedFields << "x" << "y";
    return expectedFields;
}



Event *WeatherTriggerEvent::duplicate() {
    WeatherTriggerEvent *copy = new WeatherTriggerEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setWeather(this->getWeather());

    copy->setCustomValues(this->getCustomValues());

    return copy;
}

EventFrame *WeatherTriggerEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new WeatherTriggerFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object WeatherTriggerEvent::buildEventJson(Project *) {
    OrderedJson::object weatherJson;

    weatherJson["type"] = "weather";
    weatherJson["x"] = this->getX();
    weatherJson["y"] = this->getY();
    weatherJson["elevation"] = this->getElevation();
    weatherJson["weather"] = this->getWeather();

    this->addCustomValuesTo(&weatherJson);

    return weatherJson;
}

bool WeatherTriggerEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(json["x"].toInt());
    this->setY(json["y"].toInt());
    this->setElevation(json["elevation"].toInt());
    this->setWeather(json["weather"].toString());

    this->readCustomValues(json);

    return true;
}

void WeatherTriggerEvent::setDefaultValues(Project *project) {
    this->setWeather(project->coordEventWeatherNames.first());
    this->setElevation(0);
}

const QSet<QString> expectedWeatherTriggerFields = {
    "type",
    "elevation",
    "weather",
};

QSet<QString> WeatherTriggerEvent::getExpectedFields() {
    QSet<QString> expectedFields = QSet<QString>();
    expectedFields = expectedWeatherTriggerFields;
    expectedFields << "x" << "y";
    return expectedFields;
}



void BGEvent::loadPixmap(Project *project) {
    this->pixmap = project->entitiesPixmap.copy(48, 0, 16, 16);
}



Event *SignEvent::duplicate() {
    SignEvent *copy = new SignEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setFacingDirection(this->getFacingDirection());
    copy->setScriptLabel(this->getScriptLabel());

    copy->setCustomValues(this->getCustomValues());

    return copy;
}

EventFrame *SignEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new SignFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object SignEvent::buildEventJson(Project *) {
    OrderedJson::object signJson;

    signJson["type"] = "sign";
    signJson["x"] = this->getX();
    signJson["y"] = this->getY();
    signJson["elevation"] = this->getElevation();
    signJson["player_facing_dir"] = this->getFacingDirection();
    signJson["script"] = this->getScriptLabel();

    this->addCustomValuesTo(&signJson);

    return signJson;
}

bool SignEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(json["x"].toInt());
    this->setY(json["y"].toInt());
    this->setElevation(json["elevation"].toInt());
    this->setFacingDirection(json["player_facing_dir"].toString());
    this->setScriptLabel(json["script"].toString());

    this->readCustomValues(json);

    return true;
}

void SignEvent::setDefaultValues(Project *project) {
    this->setFacingDirection(project->bgEventFacingDirections.first());
    this->setScriptLabel("NULL");
    this->setElevation(0);
}

const QSet<QString> expectedSignFields = {
    "type",
    "elevation",
    "player_facing_dir",
    "script",
};

QSet<QString> SignEvent::getExpectedFields() {
    QSet<QString> expectedFields = QSet<QString>();
    expectedFields = expectedSignFields;
    expectedFields << "x" << "y";
    return expectedFields;
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

    copy->setCustomValues(this->getCustomValues());

    return copy;
}

EventFrame *HiddenItemEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new HiddenItemFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object HiddenItemEvent::buildEventJson(Project *) {
    OrderedJson::object hiddenItemJson;

    hiddenItemJson["type"] = "hidden_item";
    hiddenItemJson["x"] = this->getX();
    hiddenItemJson["y"] = this->getY();
    hiddenItemJson["elevation"] = this->getElevation();
    hiddenItemJson["item"] = this->getItem();
    hiddenItemJson["flag"] = this->getFlag();
    if (projectConfig.getHiddenItemQuantityEnabled()) {
        hiddenItemJson["quantity"] = this->getQuantity();
    }
    if (projectConfig.getHiddenItemRequiresItemfinderEnabled()) {
        hiddenItemJson["underfoot"] = this->getUnderfoot();
    }

    this->addCustomValuesTo(&hiddenItemJson);

    return hiddenItemJson;
}

bool HiddenItemEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(json["x"].toInt());
    this->setY(json["y"].toInt());
    this->setElevation(json["elevation"].toInt());
    this->setItem(json["item"].toString());
    this->setFlag(json["flag"].toString());
    if (projectConfig.getHiddenItemQuantityEnabled()) {
        this->setQuantity(json["quantity"].toInt());
    }
    if (projectConfig.getHiddenItemRequiresItemfinderEnabled()) {
        this->setUnderfoot(json["underfoot"].toBool());
    }

    this->readCustomValues(json);

    return true;
}

void HiddenItemEvent::setDefaultValues(Project *project) {
    this->setItem(project->itemNames.first());
    this->setFlag(project->flagNames.first());
    if (projectConfig.getHiddenItemQuantityEnabled()) {
        this->setQuantity(1);
    }
    if (projectConfig.getHiddenItemRequiresItemfinderEnabled()) {
        this->setUnderfoot(false);
    }
}

const QSet<QString> expectedHiddenItemFields = {
    "type",
    "elevation",
    "item",
    "flag",
};

QSet<QString> HiddenItemEvent::getExpectedFields() {
    QSet<QString> expectedFields = QSet<QString>();
    expectedFields = expectedHiddenItemFields;
    if (projectConfig.getHiddenItemQuantityEnabled()) {
        expectedFields << "quantity";
    }
    if (projectConfig.getHiddenItemRequiresItemfinderEnabled()) {
        expectedFields << "underfoot";
    }
    expectedFields << "x" << "y";
    return expectedFields;
}



Event *SecretBaseEvent::duplicate() {
    SecretBaseEvent *copy = new SecretBaseEvent();

    copy->setX(this->getX());
    copy->setY(this->getY());
    copy->setElevation(this->getElevation());
    copy->setBaseID(this->getBaseID());

    copy->setCustomValues(this->getCustomValues());

    return copy;
}

EventFrame *SecretBaseEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new SecretBaseFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object SecretBaseEvent::buildEventJson(Project *) {
    OrderedJson::object secretBaseJson;

    secretBaseJson["type"] = "secret_base";
    secretBaseJson["x"] = this->getX();
    secretBaseJson["y"] = this->getY();
    secretBaseJson["elevation"] = this->getElevation();
    secretBaseJson["secret_base_id"] = this->getBaseID();

    this->addCustomValuesTo(&secretBaseJson);

    return secretBaseJson;
}

bool SecretBaseEvent::loadFromJson(QJsonObject json, Project *) {
    this->setX(json["x"].toInt());
    this->setY(json["y"].toInt());
    this->setElevation(json["elevation"].toInt());
    this->setBaseID(json["secret_base_id"].toString());

    this->readCustomValues(json);

    return true;
}

void SecretBaseEvent::setDefaultValues(Project *project) {
    this->setBaseID(project->secretBaseIds.first());
    this->setElevation(0);
}

const QSet<QString> expectedSecretBaseFields = {
    "type",
    "elevation",
    "secret_base_id",
};

QSet<QString> SecretBaseEvent::getExpectedFields() {
    QSet<QString> expectedFields = QSet<QString>();
    expectedFields = expectedSecretBaseFields;
    expectedFields << "x" << "y";
    return expectedFields;
}



EventFrame *HealLocationEvent::createEventFrame() {
    if (!this->eventFrame) {
        this->eventFrame = new HealLocationFrame(this);
        this->eventFrame->setup();

        QObject::connect(this->eventFrame, &QObject::destroyed, [this](){ this->eventFrame = nullptr; });
    }
    return this->eventFrame;
}

OrderedJson::object HealLocationEvent::buildEventJson(Project *) {
    return OrderedJson::object();
}

void HealLocationEvent::setDefaultValues(Project *) {
    if (this->getMap()) {
        this->setLocationName(Map::mapConstantFromName(this->getMap()->name).remove(0,4));
        this->setIdName(this->getMap()->name.replace(QRegularExpression("([a-z])([A-Z])"), "\\1_\\2").toUpper());
    }
    this->setElevation(3);
    if (projectConfig.getHealLocationRespawnDataEnabled()) {
        if (this->getMap()) this->setRespawnMap(this->getMap()->name);
        this->setRespawnNPC(1);
    }
}

void HealLocationEvent::loadPixmap(Project *project) {
    this->pixmap = project->entitiesPixmap.copy(64, 0, 16, 16);
}
