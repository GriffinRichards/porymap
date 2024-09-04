#include "config.h"
#include "metatile.h"
#include "log.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

ProjectConfig projectConfig;

const QString baseFilepath = "porymap.project.json";
const QString legacyCfgFilepath = "porymap.project.cfg";

// In both versions the default new map border is a generic tree
const QList<uint16_t> defaultBorder_RSE = {0x1D4, 0x1D5, 0x1DC, 0x1DD};
const QList<uint16_t> defaultBorder_FRLG = {0x14, 0x15, 0x1C, 0x1D};

const QSet<uint32_t> defaultWarpBehaviors_RSE = {
    0x0E, // MB_MOSSDEEP_GYM_WARP
    0x0F, // MB_MT_PYRE_HOLE
    0x1B, // MB_STAIRS_OUTSIDE_ABANDONED_SHIP
    0x1C, // MB_SHOAL_CAVE_ENTRANCE
    0x29, // MB_LAVARIDGE_GYM_B1F_WARP
    0x60, // MB_NON_ANIMATED_DOOR
    0x61, // MB_LADDER
    0x62, // MB_EAST_ARROW_WARP
    0x63, // MB_WEST_ARROW_WARP
    0x64, // MB_NORTH_ARROW_WARP
    0x65, // MB_SOUTH_ARROW_WARP
    0x67, // MB_AQUA_HIDEOUT_WARP
    0x68, // MB_LAVARIDGE_GYM_1F_WARP
    0x69, // MB_ANIMATED_DOOR
    0x6A, // MB_UP_ESCALATOR
    0x6B, // MB_DOWN_ESCALATOR
    0x6C, // MB_WATER_DOOR
    0x6D, // MB_WATER_SOUTH_ARROW_WARP
    0x6E, // MB_DEEP_SOUTH_WARP
    0x70, // MB_UNION_ROOM_WARP
    0x8D, // MB_PETALBURG_GYM_DOOR
    0x91, // MB_SECRET_BASE_SPOT_RED_CAVE_OPEN
    0x93, // MB_SECRET_BASE_SPOT_BROWN_CAVE_OPEN
    0x95, // MB_SECRET_BASE_SPOT_YELLOW_CAVE_OPEN
    0x97, // MB_SECRET_BASE_SPOT_TREE_LEFT_OPEN
    0x99, // MB_SECRET_BASE_SPOT_SHRUB_OPEN
    0x9B, // MB_SECRET_BASE_SPOT_BLUE_CAVE_OPEN
    0x9D, // MB_SECRET_BASE_SPOT_TREE_RIGHT_OPEN
};

const QSet<uint32_t> defaultWarpBehaviors_FRLG = {
    0x60, // MB_CAVE_DOOR
    0x61, // MB_LADDER
    0x62, // MB_EAST_ARROW_WARP
    0x63, // MB_WEST_ARROW_WARP
    0x64, // MB_NORTH_ARROW_WARP
    0x65, // MB_SOUTH_ARROW_WARP
    0x66, // MB_FALL_WARP
    0x67, // MB_REGULAR_WARP
    0x68, // MB_LAVARIDGE_1F_WARP
    0x69, // MB_WARP_DOOR
    0x6A, // MB_UP_ESCALATOR
    0x6B, // MB_DOWN_ESCALATOR
    0x6C, // MB_UP_RIGHT_STAIR_WARP
    0x6D, // MB_UP_LEFT_STAIR_WARP
    0x6E, // MB_DOWN_RIGHT_STAIR_WARP
    0x6F, // MB_DOWN_LEFT_STAIR_WARP
    0x71, // MB_UNION_ROOM_WARP
};

// TODO: symbol_wild_encounters should ultimately be removed from the table below. We can determine this name when we read the project.
const QMap<ProjectIdentifier, QPair<QString, QString>> ProjectConfig::defaultIdentifiers = {
    // Symbols
    {ProjectIdentifier::symbol_facing_directions,      {"symbol_facing_directions",      "gInitialMovementTypeFacingDirections"}},
    {ProjectIdentifier::symbol_obj_event_gfx_pointers, {"symbol_obj_event_gfx_pointers", "gObjectEventGraphicsInfoPointers"}},
    {ProjectIdentifier::symbol_pokemon_icon_table,     {"symbol_pokemon_icon_table",     "gMonIconTable"}},
    {ProjectIdentifier::symbol_wild_encounters,        {"symbol_wild_encounters",        "gWildMonHeaders"}},
    {ProjectIdentifier::symbol_heal_locations_type,    {"symbol_heal_locations_type",    "struct HealLocation"}},
    {ProjectIdentifier::symbol_heal_locations,         {"symbol_heal_locations",         "sHealLocations"}},
    {ProjectIdentifier::symbol_spawn_points,           {"symbol_spawn_points",           "sSpawnPoints"}},
    {ProjectIdentifier::symbol_spawn_maps,             {"symbol_spawn_maps",             "u16 sWhiteoutRespawnHealCenterMapIdxs"}},
    {ProjectIdentifier::symbol_spawn_npcs,             {"symbol_spawn_npcs",             "u8 sWhiteoutRespawnHealerNpcIds"}},
    {ProjectIdentifier::symbol_attribute_table,        {"symbol_attribute_table",        "sMetatileAttrMasks"}},
    {ProjectIdentifier::symbol_tilesets_prefix,        {"symbol_tilesets_prefix",        "gTileset_"}},
    // Defines
    {ProjectIdentifier::define_obj_event_count,        {"define_obj_event_count",        "OBJECT_EVENT_TEMPLATES_COUNT"}},
    {ProjectIdentifier::define_min_level,              {"define_min_level",              "MIN_LEVEL"}},
    {ProjectIdentifier::define_max_level,              {"define_max_level",              "MAX_LEVEL"}},
    {ProjectIdentifier::define_max_encounter_rate,     {"define_max_encounter_rate",     "MAX_ENCOUNTER_RATE"}},
    {ProjectIdentifier::define_tiles_primary,          {"define_tiles_primary",          "NUM_TILES_IN_PRIMARY"}},
    {ProjectIdentifier::define_tiles_total,            {"define_tiles_total",            "NUM_TILES_TOTAL"}},
    {ProjectIdentifier::define_metatiles_primary,      {"define_metatiles_primary",      "NUM_METATILES_IN_PRIMARY"}},
    {ProjectIdentifier::define_pals_primary,           {"define_pals_primary",           "NUM_PALS_IN_PRIMARY"}},
    {ProjectIdentifier::define_pals_total,             {"define_pals_total",             "NUM_PALS_TOTAL"}},
    {ProjectIdentifier::define_tiles_per_metatile,     {"define_tiles_per_metatile",     "NUM_TILES_PER_METATILE"}},
    {ProjectIdentifier::define_map_size,               {"define_map_size",               "MAX_MAP_DATA_SIZE"}},
    {ProjectIdentifier::define_mask_metatile,          {"define_mask_metatile",          "MAPGRID_METATILE_ID_MASK"}},
    {ProjectIdentifier::define_mask_collision,         {"define_mask_collision",         "MAPGRID_COLLISION_MASK"}},
    {ProjectIdentifier::define_mask_elevation,         {"define_mask_elevation",         "MAPGRID_ELEVATION_MASK"}},
    {ProjectIdentifier::define_mask_behavior,          {"define_mask_behavior",          "METATILE_ATTR_BEHAVIOR_MASK"}},
    {ProjectIdentifier::define_mask_layer,             {"define_mask_layer",             "METATILE_ATTR_LAYER_MASK"}},
    {ProjectIdentifier::define_attribute_behavior,     {"define_attribute_behavior",     "METATILE_ATTRIBUTE_BEHAVIOR"}},
    {ProjectIdentifier::define_attribute_layer,        {"define_attribute_layer",        "METATILE_ATTRIBUTE_LAYER_TYPE"}},
    {ProjectIdentifier::define_attribute_terrain,      {"define_attribute_terrain",      "METATILE_ATTRIBUTE_TERRAIN"}},
    {ProjectIdentifier::define_attribute_encounter,    {"define_attribute_encounter",    "METATILE_ATTRIBUTE_ENCOUNTER_TYPE"}},
    {ProjectIdentifier::define_metatile_label_prefix,  {"define_metatile_label_prefix",  "METATILE_"}},
    {ProjectIdentifier::define_heal_locations_prefix,  {"define_heal_locations_prefix",  "HEAL_LOCATION_"}},
    {ProjectIdentifier::define_spawn_prefix,           {"define_spawn_prefix",           "SPAWN_"}},
    {ProjectIdentifier::define_map_prefix,             {"define_map_prefix",             "MAP_"}},
    {ProjectIdentifier::define_map_dynamic,            {"define_map_dynamic",            "DYNAMIC"}},
    {ProjectIdentifier::define_map_empty,              {"define_map_empty",              "UNDEFINED"}},
    {ProjectIdentifier::define_map_section_prefix,     {"define_map_section_prefix",     "MAPSEC_"}},
    {ProjectIdentifier::define_map_section_empty,      {"define_map_section_empty",      "NONE"}},
    {ProjectIdentifier::define_map_section_count,      {"define_map_section_count",      "COUNT"}},
    {ProjectIdentifier::define_species_prefix,         {"define_species_prefix",         "SPECIES_"}},
    // Regex
    {ProjectIdentifier::regex_behaviors,               {"regex_behaviors",               "\\bMB_"}},
    {ProjectIdentifier::regex_obj_event_gfx,           {"regex_obj_event_gfx",           "\\bOBJ_EVENT_GFX_"}},
    {ProjectIdentifier::regex_items,                   {"regex_items",                   "\\bITEM_(?!(B_)?USE_)"}}, // Exclude ITEM_USE_ and ITEM_B_USE_ constants
    {ProjectIdentifier::regex_flags,                   {"regex_flags",                   "\\bFLAG_"}},
    {ProjectIdentifier::regex_vars,                    {"regex_vars",                    "\\bVAR_"}},
    {ProjectIdentifier::regex_movement_types,          {"regex_movement_types",          "\\bMOVEMENT_TYPE_"}},
    {ProjectIdentifier::regex_map_types,               {"regex_map_types",               "\\bMAP_TYPE_"}},
    {ProjectIdentifier::regex_battle_scenes,           {"regex_battle_scenes",           "\\bMAP_BATTLE_SCENE_"}},
    {ProjectIdentifier::regex_weather,                 {"regex_weather",                 "\\bWEATHER_"}},
    {ProjectIdentifier::regex_coord_event_weather,     {"regex_coord_event_weather",     "\\bCOORD_EVENT_WEATHER_"}},
    {ProjectIdentifier::regex_secret_bases,            {"regex_secret_bases",            "\\bSECRET_BASE_[A-Za-z0-9_]*_[0-9]+"}},
    {ProjectIdentifier::regex_sign_facing_directions,  {"regex_sign_facing_directions",  "\\bBG_EVENT_PLAYER_FACING_"}},
    {ProjectIdentifier::regex_trainer_types,           {"regex_trainer_types",           "\\bTRAINER_TYPE_"}},
    {ProjectIdentifier::regex_music,                   {"regex_music",                   "\\b(SE|MUS)_"}},
};

const QMap<ProjectFilePath, QPair<QString, QString>> ProjectConfig::defaultPaths = {
    {ProjectFilePath::data_map_folders,                 { "data_map_folders",                "data/maps/"}},
    {ProjectFilePath::data_scripts_folders,             { "data_scripts_folders",            "data/scripts/"}},
    {ProjectFilePath::data_layouts_folders,             { "data_layouts_folders",            "data/layouts/"}},
    {ProjectFilePath::data_tilesets_folders,            { "data_tilesets_folders",           "data/tilesets/"}},
    {ProjectFilePath::data_event_scripts,               { "data_event_scripts",              "data/event_scripts.s"}},
    {ProjectFilePath::json_map_groups,                  { "json_map_groups",                 "data/maps/map_groups.json"}},
    {ProjectFilePath::json_layouts,                     { "json_layouts",                    "data/layouts/layouts.json"}},
    {ProjectFilePath::json_wild_encounters,             { "json_wild_encounters",            "src/data/wild_encounters.json"}},
    {ProjectFilePath::json_region_map_entries,          { "json_region_map_entries",         "src/data/region_map/region_map_sections.json"}},
    {ProjectFilePath::json_region_porymap_cfg,          { "json_region_porymap_cfg",         "src/data/region_map/porymap_config.json"}},
    {ProjectFilePath::tilesets_headers,                 { "tilesets_headers",                "src/data/tilesets/headers.h"}},
    {ProjectFilePath::tilesets_graphics,                { "tilesets_graphics",               "src/data/tilesets/graphics.h"}},
    {ProjectFilePath::tilesets_metatiles,               { "tilesets_metatiles",              "src/data/tilesets/metatiles.h"}},
    {ProjectFilePath::tilesets_headers_asm,             { "tilesets_headers_asm",            "data/tilesets/headers.inc"}},
    {ProjectFilePath::tilesets_graphics_asm,            { "tilesets_graphics_asm",           "data/tilesets/graphics.inc"}},
    {ProjectFilePath::tilesets_metatiles_asm,           { "tilesets_metatiles_asm",          "data/tilesets/metatiles.inc"}},
    {ProjectFilePath::data_obj_event_gfx_pointers,      { "data_obj_event_gfx_pointers",     "src/data/object_events/object_event_graphics_info_pointers.h"}},
    {ProjectFilePath::data_obj_event_gfx_info,          { "data_obj_event_gfx_info",         "src/data/object_events/object_event_graphics_info.h"}},
    {ProjectFilePath::data_obj_event_pic_tables,        { "data_obj_event_pic_tables",       "src/data/object_events/object_event_pic_tables.h"}},
    {ProjectFilePath::data_obj_event_gfx,               { "data_obj_event_gfx",              "src/data/object_events/object_event_graphics.h"}},
    {ProjectFilePath::data_pokemon_gfx,                 { "data_pokemon_gfx",                "src/data/graphics/pokemon.h"}},
    {ProjectFilePath::data_heal_locations,              { "data_heal_locations",             "src/data/heal_locations.h"}},
    {ProjectFilePath::constants_global,                 { "constants_global",                "include/constants/global.h"}},
    {ProjectFilePath::constants_map_groups,             { "constants_map_groups",            "include/constants/map_groups.h"}},
    {ProjectFilePath::constants_items,                  { "constants_items",                 "include/constants/items.h"}},
    {ProjectFilePath::constants_flags,                  { "constants_flags",                 "include/constants/flags.h"}},
    {ProjectFilePath::constants_vars,                   { "constants_vars",                  "include/constants/vars.h"}},
    {ProjectFilePath::constants_weather,                { "constants_weather",               "include/constants/weather.h"}},
    {ProjectFilePath::constants_songs,                  { "constants_songs",                 "include/constants/songs.h"}},
    {ProjectFilePath::constants_heal_locations,         { "constants_heal_locations",        "include/constants/heal_locations.h"}},
    {ProjectFilePath::constants_pokemon,                { "constants_pokemon",               "include/constants/pokemon.h"}},
    {ProjectFilePath::constants_map_types,              { "constants_map_types",             "include/constants/map_types.h"}},
    {ProjectFilePath::constants_trainer_types,          { "constants_trainer_types",         "include/constants/trainer_types.h"}},
    {ProjectFilePath::constants_secret_bases,           { "constants_secret_bases",          "include/constants/secret_bases.h"}},
    {ProjectFilePath::constants_obj_event_movement,     { "constants_obj_event_movement",    "include/constants/event_object_movement.h"}},
    {ProjectFilePath::constants_obj_events,             { "constants_obj_events",            "include/constants/event_objects.h"}},
    {ProjectFilePath::constants_event_bg,               { "constants_event_bg",              "include/constants/event_bg.h"}},
    {ProjectFilePath::constants_region_map_sections,    { "constants_region_map_sections",   "include/constants/region_map_sections.h"}},
    {ProjectFilePath::constants_metatile_labels,        { "constants_metatile_labels",       "include/constants/metatile_labels.h"}},
    {ProjectFilePath::constants_metatile_behaviors,     { "constants_metatile_behaviors",    "include/constants/metatile_behaviors.h"}},
    {ProjectFilePath::constants_species,                { "constants_species",               "include/constants/species.h"}},
    {ProjectFilePath::constants_fieldmap,               { "constants_fieldmap",              "include/fieldmap.h"}},
    {ProjectFilePath::global_fieldmap,                  { "global_fieldmap",                 "include/global.fieldmap.h"}},
    {ProjectFilePath::fieldmap,                         { "fieldmap",                        "src/fieldmap.c"}},
    {ProjectFilePath::pokemon_icon_table,               { "pokemon_icon_table",              "src/pokemon_icon.c"}},
    {ProjectFilePath::initial_facing_table,             { "initial_facing_table",            "src/event_object_movement.c"}},
    {ProjectFilePath::wild_encounter,                   { "wild_encounter",                  "src/wild_encounter.c"}},
    {ProjectFilePath::pokemon_gfx,                      { "pokemon_gfx",                     "graphics/pokemon/"}},
};


const QStringList ProjectConfig::versionStrings = {
    "pokeruby",
    "pokefirered",
    "pokeemerald",
};

const QMap<BaseGameVersion, QString> baseGameVersionMap = {
    {BaseGameVersion::pokeruby, ProjectConfig::versionStrings[0]},
    {BaseGameVersion::pokefirered, ProjectConfig::versionStrings[1]},
    {BaseGameVersion::pokeemerald, ProjectConfig::versionStrings[2]},
};

const QMap<BaseGameVersion, QStringList> versionDetectNames = {
    {BaseGameVersion::pokeruby, {"ruby", "sapphire"}},
    {BaseGameVersion::pokefirered, {"firered", "leafgreen"}},
    {BaseGameVersion::pokeemerald, {"emerald"}},
};

// If a string exclusively contains one version name we assume its identity,
// otherwise we leave it unknown and we'll need the user to tell us the version.
BaseGameVersion ProjectConfig::stringToBaseGameVersion(const QString &string) {
    BaseGameVersion version = BaseGameVersion::none;
    for (auto i = versionDetectNames.cbegin(), end = versionDetectNames.cend(); i != end; i++) {
        // Compare the given string to all the possible names for this game version
        const QStringList names = i.value();
        for (auto name : names) {
            if (string.contains(name)) {
                if (version != BaseGameVersion::none) {
                    // The given string matches multiple versions, so we can't be sure which it is.
                    return BaseGameVersion::none;
                }
                version = i.key();
                break;
            }
        }
    }
    // We finished checking the names for each version; the name either matched 1 version or none.
    return version;
}
/*
QString ProjectConfig::getConfigFilepath() {
    // porymap config file is in the same directory as porymap itself.
    return QDir(this->projectDir).filePath("porymap.project.cfg");
}*/

ProjectIdentifier reverseDefaultIdentifier(QString str) {
    for (auto i = ProjectConfig::defaultIdentifiers.cbegin(), end = ProjectConfig::defaultIdentifiers.cend(); i != end; i++) {
        if (i.value().first == str) return i.key();
    }
    return static_cast<ProjectIdentifier>(-1);
}

ProjectFilePath reverseDefaultPaths(QString str) {
    for (auto it = ProjectConfig::defaultPaths.constKeyValueBegin(); it != ProjectConfig::defaultPaths.constKeyValueEnd(); ++it) {
        if ((*it).second.first == str) return (*it).first;
    }
    return static_cast<ProjectFilePath>(-1);
}

ProjectConfig::ProjectConfig(const QString &projectDir) {
    this->projectDir = projectDir;

    // TODO:
    // - In project dir, check for new file path
    // - If not present, check for cfg file. Convert to QJsonDocument.
    // - If present, parse JSON file to QJsonDocument. Report errors if necessary.
    // - With QJsonDocument, first get BaseGameVersion
    // - If not present, try to interpret from projectDir, or show dialogue to set BaseGameVersion.
    BaseGameVersion version = BaseGameVersion::pokeemerald;
    this->reset(version);
    // - Overwrite defaults using data in QJsonDocument

}

ProjectConfig::ProjectConfig(BaseGameVersion version) {
    this->projectDir = "";
    this->reset(version);
}

void ProjectConfig::reset(BaseGameVersion version) {
    this->baseGameVersion = version;

    // Version-specific defaults
    bool isPokefirered = this->baseGameVersion == BaseGameVersion::pokefirered;
    this->useCustomBorderSize = isPokefirered;
    this->eventWeatherTriggerEnabled = !isPokefirered;
    this->eventSecretBaseEnabled = !isPokefirered;
    this->hiddenItemQuantityEnabled = isPokefirered;
    this->hiddenItemRequiresItemfinderEnabled = isPokefirered;
    this->healLocationRespawnDataEnabled = isPokefirered;
    this->eventCloneObjectEnabled = isPokefirered;
    this->floorNumberEnabled = isPokefirered;
    this->createMapTextFileEnabled = (this->baseGameVersion != BaseGameVersion::pokeemerald);
    this->newMapBorderMetatileIds = isPokefirered ? defaultBorder_FRLG : defaultBorder_RSE;
    this->defaultSecondaryTileset = isPokefirered ? "gTileset_PalletTown" : "gTileset_Petalburg";
    this->metatileAttributesSize = Metatile::getDefaultAttributesSize(this->baseGameVersion);
    this->metatileBehaviorMask = Metatile::getDefaultAttributesMask(this->baseGameVersion, Metatile::Attr::Behavior);
    this->metatileTerrainTypeMask = Metatile::getDefaultAttributesMask(this->baseGameVersion, Metatile::Attr::TerrainType);
    this->metatileEncounterTypeMask = Metatile::getDefaultAttributesMask(this->baseGameVersion, Metatile::Attr::EncounterType);
    this->metatileLayerTypeMask = Metatile::getDefaultAttributesMask(this->baseGameVersion, Metatile::Attr::LayerType);
    this->mapAllowFlagsEnabled = (this->baseGameVersion != BaseGameVersion::pokeruby);
    this->warpBehaviors = isPokefirered ? defaultWarpBehaviors_FRLG : defaultWarpBehaviors_RSE;

    // Version-agnostic defaults
    this->usePoryScript = false;
    this->tripleLayerMetatilesEnabled = false;
    this->defaultMetatileId = 1;
    this->defaultElevation = 3;
    this->defaultCollision = 0;
    this->defaultPrimaryTileset = "gTileset_General";
    this->prefabFilepath = QString(); // TODO: Use actual default
    this->prefabImportPrompted = false; // TODO: Remove
    this->tilesetsHaveCallback = true;
    this->tilesetsHaveIsCompressed = true;
    this->filePaths.clear();
    this->eventIconPaths.clear();
    this->pokemonIconPaths.clear();
    this->collisionSheetPath = QString(); // TODO: Use actual default?
    this->collisionSheetWidth = 2;
    this->collisionSheetHeight = 16;
    this->blockMetatileIdMask = 0x03FF;
    this->blockCollisionMask = 0x0C00;
    this->blockElevationMask = 0xF000;
    this->identifiers.clear();
    this->defaultEventCustomAttributes.clear();
    this->defaultMapCustomAttributes.clear();
}


void ProjectConfig::setFilePath(ProjectFilePath pathId, const QString &path) {
    if (!defaultPaths.contains(pathId)) return;
    if (path.isEmpty()) {
        this->filePaths.remove(pathId);
    } else {
        this->filePaths[pathId] = path;
    }
}

void ProjectConfig::setFilePath(const QString &pathId, const QString &path) {
    this->setFilePath(reverseDefaultPaths(pathId), path);
}

QString ProjectConfig::getCustomFilePath(ProjectFilePath pathId) {
    return this->filePaths.value(pathId);
}

QString ProjectConfig::getCustomFilePath(const QString &pathId) {
    return this->getCustomFilePath(reverseDefaultPaths(pathId));
}

QString ProjectConfig::getFilePath(ProjectFilePath pathId) {
    const QString customPath = this->getCustomFilePath(pathId);
    if (!customPath.isEmpty()) {
        // A custom filepath has been specified. If the file/folder exists, use that.
        const QString absCustomPath = this->projectDir + QDir::separator() + customPath;
        if (QFileInfo::exists(absCustomPath)) {
            return customPath;
        } else {
            logError(QString("Custom project filepath '%1' not found. Using default.").arg(absCustomPath));
        }
    }
    return defaultPaths.contains(pathId) ? defaultPaths[pathId].second : QString();

}

void ProjectConfig::setIdentifier(ProjectIdentifier id, const QString &text) {
    if (!defaultIdentifiers.contains(id)) return;
    QString copy(text);
    if (copy.isEmpty()) {
        this->identifiers.remove(id);
    } else {
        this->identifiers[id] = copy;
    }
}

void ProjectConfig::setIdentifier(const QString &id, const QString &text) {
    this->setIdentifier(reverseDefaultIdentifier(id), text);
}

QString ProjectConfig::getCustomIdentifier(ProjectIdentifier id) {
    return this->identifiers.value(id);
}

QString ProjectConfig::getCustomIdentifier(const QString &id) {
    return this->getCustomIdentifier(reverseDefaultIdentifier(id));
}

QString ProjectConfig::getIdentifier(ProjectIdentifier id) {
    const QString customText = this->getCustomIdentifier(id);
    if (!customText.isEmpty())
        return customText;
    return defaultIdentifiers.contains(id) ? defaultIdentifiers[id].second : QString();
}

QString ProjectConfig::getBaseGameVersionString(BaseGameVersion version) {
    if (!baseGameVersionMap.contains(version)) {
        version = BaseGameVersion::pokeemerald;
    }
    return baseGameVersionMap.value(version);
}

QString ProjectConfig::getBaseGameVersionString() {
    return this->getBaseGameVersionString(this->baseGameVersion);
}

int ProjectConfig::getNumLayersInMetatile() {
    return this->tripleLayerMetatilesEnabled ? 3 : 2;
}

int ProjectConfig::getNumTilesInMetatile() {
    return this->tripleLayerMetatilesEnabled ? 12 : 8;
}

void ProjectConfig::setEventIconPath(Event::Group group, const QString &path) {
    this->eventIconPaths[group] = path;
}

QString ProjectConfig::getEventIconPath(Event::Group group) {
    return this->eventIconPaths.value(group);
}

void ProjectConfig::setPokemonIconPath(const QString &species, const QString &path) {
    this->pokemonIconPaths[species] = path;
}

QString ProjectConfig::getPokemonIconPath(const QString &species) {
    return this->pokemonIconPaths.value(species);
}

QHash<QString, QString> ProjectConfig::getPokemonIconPaths() {
    return this->pokemonIconPaths;
}

void ProjectConfig::insertDefaultEventCustomAttribute(Event::Type eventType, const QString &key, QJsonValue value) {
    this->defaultEventCustomAttributes[eventType].insert(key, value);
}

void ProjectConfig::insertDefaultMapCustomAttribute(const QString &key, QJsonValue value) {
    this->defaultMapCustomAttributes.insert(key, value);
}

void ProjectConfig::removeDefaultEventCustomAttribute(Event::Type eventType, const QString &key) {
    this->defaultEventCustomAttributes[eventType].remove(key);
}

void ProjectConfig::removeDefaultMapCustomAttribute(const QString &key) {
    this->defaultMapCustomAttributes.remove(key);
}

QMap<QString, QJsonValue> ProjectConfig::getDefaultEventCustomAttributes(Event::Type eventType) {
    return this->defaultEventCustomAttributes.value(eventType);
}

QMap<QString, QJsonValue> ProjectConfig::getDefaultMapCustomAttributes() {
    return this->defaultMapCustomAttributes;
}

void ProjectConfig::parseCustomAttributes(const QString &key, const QString &value) {
    static const QRegularExpression regex("custom_attributes/(?<identifier>\\w+)");
    auto match = regex.match(key);
    if (!match.hasMatch()){
        //logInvalidKey(key);
        return;
    }

    // Value should be a comma-separated list of sequences of the form 'key:type:value'.
    // Some day if this config file is formatted as JSON data we wouldn't need to store 'type' (among other simplifications).
    QMap<QString, QJsonValue> map;
    const QStringList attributeSequences = value.split(",", Qt::SkipEmptyParts);
    if (attributeSequences.isEmpty())
        return;
    for (auto sequence : attributeSequences) {
        // Parse each 'type:key:value' sequence
        const QStringList attributeData = sequence.split(":");
        if (attributeData.length() != 3) {
            logWarn(QString("Invalid value '%1' for custom attribute in '%2'").arg(sequence).arg(key));
            continue;
        }
        const QString attrKey = attributeData.at(0);
        const QString attrType = attributeData.at(1);
        const QString attrValue = attributeData.at(2);

        QJsonValue value;
        if (attrType == "string") {
            value = QJsonValue(attrValue);
        } else if (attrType == "number") {
            bool ok;
            int num = attrValue.toInt(&ok, 0);
            if (!ok)
                logWarn(QString("Invalid value '%1' for custom attribute '%2' in '%3'").arg(attrValue).arg(attrKey).arg(key));
            value = QJsonValue(num);
        } else if (attrType == "bool") {
            bool ok;
            int num = attrValue.toInt(&ok, 0);
            if (!ok || (num != 0 && num != 1))
                logWarn(QString("Invalid value '%1' for custom attribute '%2' in '%3'").arg(attrValue).arg(attrKey).arg(key));
            value = QJsonValue(num == 1);
        } else {
            logWarn(QString("Invalid value type '%1' for custom attribute '%2' in '%3'").arg(attrType).arg(attrKey).arg(key));
            continue;
        }
        // Successfully parsed a 'type:key:value' sequence
        map.insert(attrKey, value);
    }

    // Determine who the custom attribute map belongs to (either the map header or some Event type)
    const QString identifier = match.captured("identifier");

    if (identifier == "header") {
        this->defaultMapCustomAttributes = map;
        return;
    }

    Event::Type eventType = Event::eventTypeFromString(identifier);
    if (eventType != Event::Type::None) {
        this->defaultEventCustomAttributes[eventType] = map;
        return;
    }

    logWarn(QString("Invalid custom attributes identifier '%1' in '%2'").arg(identifier).arg(key));
}

// Assemble comma-separated list of sequences of the form 'key:type:value'.
QString ProjectConfig::customAttributesToString(const QMap<QString, QJsonValue> attributes) {
    QStringList output;
    for (auto i = attributes.cbegin(), end = attributes.cend(); i != end; i++) {
        QString value;
        QString typeStr;
        QJsonValue::Type type = i.value().type();
        if (type == QJsonValue::Type::String) {
            typeStr = "string";
            value = i.value().toString();
        } else if (type == QJsonValue::Type::Double) {
            typeStr = "number";
            value = QString::number(i.value().toInt());
        } else if (type == QJsonValue::Type::Bool) {
            typeStr = "bool";
            value = QString::number(i.value().toBool());
        } else {
            continue;
        }
        output.append(QString("%1:%2:%3").arg(i.key()).arg(typeStr).arg(value));
    }
    return output.join(",");
}

/*

void ProjectConfig::parseConfigKeyValue(QString key, QString value) {
    if (key == "base_game_version") {
        this->baseGameVersion = this->stringToBaseGameVersion(value.toLower());
        if (this->baseGameVersion == BaseGameVersion::none) {
            logWarn(QString("Invalid config value for base_game_version: '%1'. Must be 'pokeruby', 'pokefirered' or 'pokeemerald'.").arg(value));
            this->baseGameVersion = BaseGameVersion::pokeemerald;
        }
    } else if (key == "use_poryscript") {
        this->usePoryScript = getConfigBool(key, value);
    } else if (key == "use_custom_border_size") {
        this->useCustomBorderSize = getConfigBool(key, value);
    } else if (key == "enable_event_weather_trigger") {
        this->eventWeatherTriggerEnabled = getConfigBool(key, value);
    } else if (key == "enable_event_secret_base") {
        this->eventSecretBaseEnabled = getConfigBool(key, value);
    } else if (key == "enable_hidden_item_quantity") {
        this->hiddenItemQuantityEnabled = getConfigBool(key, value);
    } else if (key == "enable_hidden_item_requires_itemfinder") {
        this->hiddenItemRequiresItemfinderEnabled = getConfigBool(key, value);
    } else if (key == "enable_heal_location_respawn_data") {
        this->healLocationRespawnDataEnabled = getConfigBool(key, value);
    } else if (key == "enable_event_clone_object" || key == "enable_object_event_in_connection") {
        this->eventCloneObjectEnabled = getConfigBool(key, value);
        key = "enable_event_clone_object"; // Backwards compatibiliy, replace old name above
    } else if (key == "enable_floor_number") {
        this->floorNumberEnabled = getConfigBool(key, value);
    } else if (key == "create_map_text_file") {
        this->createMapTextFileEnabled = getConfigBool(key, value);
    } else if (key == "enable_triple_layer_metatiles") {
        this->tripleLayerMetatilesEnabled = getConfigBool(key, value);
    } else if (key == "default_metatile") {
        this->defaultMetatileId = getConfigUint32(key, value, 0, Block::maxValue);
    } else if (key == "default_elevation") {
        this->defaultElevation = getConfigUint32(key, value, 0, Block::maxValue);
    } else if (key == "default_collision") {
        this->defaultCollision = getConfigUint32(key, value, 0, Block::maxValue);
    } else if (key == "new_map_border_metatiles") {
        this->newMapBorderMetatileIds.clear();
        QList<QString> metatileIds = value.split(",");
        for (int i = 0; i < metatileIds.size(); i++) {
            int metatileId = getConfigUint32(key, metatileIds.at(i), 0, Block::maxValue);
            this->newMapBorderMetatileIds.append(metatileId);
        }
    } else if (key == "default_primary_tileset") {
        this->defaultPrimaryTileset = value;
    } else if (key == "default_secondary_tileset") {
        this->defaultSecondaryTileset = value;
    } else if (key == "metatile_attributes_size") {
        int size = getConfigInteger(key, value, 1, 4, 2);
        if (size & (size - 1)) {
            logWarn(QString("Invalid config value for %1: must be 1, 2, or 4").arg(key));
            return; // Don't set a default now, it will be set later based on the base game version
        }
        this->metatileAttributesSize = size;
    } else if (key == "metatile_behavior_mask") {
        this->metatileBehaviorMask = getConfigUint32(key, value);
    } else if (key == "metatile_terrain_type_mask") {
        this->metatileTerrainTypeMask = getConfigUint32(key, value);
    } else if (key == "metatile_encounter_type_mask") {
        this->metatileEncounterTypeMask = getConfigUint32(key, value);
    } else if (key == "metatile_layer_type_mask") {
        this->metatileLayerTypeMask = getConfigUint32(key, value);
    } else if (key == "block_metatile_id_mask") {
        this->blockMetatileIdMask = getConfigUint32(key, value, 0, Block::maxValue);
    } else if (key == "block_collision_mask") {
        this->blockCollisionMask = getConfigUint32(key, value, 0, Block::maxValue);
    } else if (key == "block_elevation_mask") {
        this->blockElevationMask = getConfigUint32(key, value, 0, Block::maxValue);
    } else if (key == "enable_map_allow_flags") {
        this->mapAllowFlagsEnabled = getConfigBool(key, value);
#ifdef CONFIG_BACKWARDS_COMPATABILITY
    } else if (key == "recent_map") {
        userConfig.recentMap = value;
    } else if (key == "use_encounter_json") {
        userConfig.useEncounterJson = getConfigBool(key, value);
    } else if (key == "custom_scripts") {
        userConfig.parseCustomScripts(value);
#endif
    } else if (key.startsWith("path/")) {
        auto k = reverseDefaultPaths(key.mid(5));
        if (k != static_cast<ProjectFilePath>(-1)) {
            this->setFilePath(k, value);
        } else {
            logWarn(QString("Invalid config key found in config file %1: '%2'").arg(this->getConfigFilepath()).arg(key));
        }
    } else if (key.startsWith("ident/")) {
        auto identifierId = reverseDefaultIdentifier(key.mid(6));
        if (identifierId != static_cast<ProjectIdentifier>(-1)) {
            this->setIdentifier(identifierId, value);
        } else {
            logWarn(QString("Invalid config key found in config file %1: '%2'").arg(this->getConfigFilepath()).arg(key));
        }
    } else if (key == "prefabs_filepath") {
        this->prefabFilepath = value;
    } else if (key == "prefabs_import_prompted") {
        this->prefabImportPrompted = getConfigBool(key, value);
    } else if (key == "tilesets_have_callback") {
        this->tilesetsHaveCallback = getConfigBool(key, value);
    } else if (key == "tilesets_have_is_compressed") {
        this->tilesetsHaveIsCompressed = getConfigBool(key, value);
    } else if (key == "event_icon_path_object") {
        this->eventIconPaths[Event::Group::Object] = value;
    } else if (key == "event_icon_path_warp") {
        this->eventIconPaths[Event::Group::Warp] = value;
    } else if (key == "event_icon_path_coord") {
        this->eventIconPaths[Event::Group::Coord] = value;
    } else if (key == "event_icon_path_bg") {
        this->eventIconPaths[Event::Group::Bg] = value;
    } else if (key == "event_icon_path_heal") {
        this->eventIconPaths[Event::Group::Heal] = value;
    } else if (key.startsWith("pokemon_icon_path/")) {
        this->pokemonIconPaths.insert(key.mid(18).toUpper(), value);
    } else if (key == "collision_sheet_path") {
        this->collisionSheetPath = value;
    } else if (key == "collision_sheet_width") {
        this->collisionSheetWidth = getConfigUint32(key, value, 1, Block::maxValue);
    } else if (key == "collision_sheet_height") {
        this->collisionSheetHeight = getConfigUint32(key, value, 1, Block::maxValue);
    } else if (key == "warp_behaviors") {
        this->warpBehaviors.clear();
        value.remove(" ");
        QStringList behaviorList = value.split(",", Qt::SkipEmptyParts);
        for (auto s : behaviorList)
            this->warpBehaviors.insert(getConfigUint32(key, s));
    } else {
        logWarn(QString("Invalid config key found in config file %1: '%2'").arg(this->getConfigFilepath()).arg(key));
    }
    readKeys.append(key);
}

QMap<QString, QString> ProjectConfig::getKeyValueMap() {
    QMap<QString, QString> map;
    map.insert("base_game_version", baseGameVersionMap.value(this->baseGameVersion));
    map.insert("use_poryscript", QString::number(this->usePoryScript));
    map.insert("use_custom_border_size", QString::number(this->useCustomBorderSize));
    map.insert("enable_event_weather_trigger", QString::number(this->eventWeatherTriggerEnabled));
    map.insert("enable_event_secret_base", QString::number(this->eventSecretBaseEnabled));
    map.insert("enable_hidden_item_quantity", QString::number(this->hiddenItemQuantityEnabled));
    map.insert("enable_hidden_item_requires_itemfinder", QString::number(this->hiddenItemRequiresItemfinderEnabled));
    map.insert("enable_heal_location_respawn_data", QString::number(this->healLocationRespawnDataEnabled));
    map.insert("enable_event_clone_object", QString::number(this->eventCloneObjectEnabled));
    map.insert("enable_floor_number", QString::number(this->floorNumberEnabled));
    map.insert("create_map_text_file", QString::number(this->createMapTextFileEnabled));
    map.insert("enable_triple_layer_metatiles", QString::number(this->tripleLayerMetatilesEnabled));
    map.insert("default_metatile", Metatile::getMetatileIdString(this->defaultMetatileId));
    map.insert("default_elevation", QString::number(this->defaultElevation));
    map.insert("default_collision", QString::number(this->defaultCollision));
    map.insert("new_map_border_metatiles", Metatile::getMetatileIdStrings(this->newMapBorderMetatileIds));
    map.insert("default_primary_tileset", this->defaultPrimaryTileset);
    map.insert("default_secondary_tileset", this->defaultSecondaryTileset);
    map.insert("prefabs_filepath", this->prefabFilepath);
    map.insert("prefabs_import_prompted", QString::number(this->prefabImportPrompted));
    for (auto it = this->filePaths.constKeyValueBegin(); it != this->filePaths.constKeyValueEnd(); ++it) {
        map.insert("path/"+defaultPaths[(*it).first].first, (*it).second);
    }
    map.insert("tilesets_have_callback", QString::number(this->tilesetsHaveCallback));
    map.insert("tilesets_have_is_compressed", QString::number(this->tilesetsHaveIsCompressed));
    map.insert("metatile_attributes_size", QString::number(this->metatileAttributesSize));
    map.insert("metatile_behavior_mask", "0x" + QString::number(this->metatileBehaviorMask, 16).toUpper());
    map.insert("metatile_terrain_type_mask", "0x" + QString::number(this->metatileTerrainTypeMask, 16).toUpper());
    map.insert("metatile_encounter_type_mask", "0x" + QString::number(this->metatileEncounterTypeMask, 16).toUpper());
    map.insert("metatile_layer_type_mask", "0x" + QString::number(this->metatileLayerTypeMask, 16).toUpper());
    map.insert("block_metatile_id_mask", "0x" + QString::number(this->blockMetatileIdMask, 16).toUpper());
    map.insert("block_collision_mask", "0x" + QString::number(this->blockCollisionMask, 16).toUpper());
    map.insert("block_elevation_mask", "0x" + QString::number(this->blockElevationMask, 16).toUpper());
    map.insert("enable_map_allow_flags", QString::number(this->mapAllowFlagsEnabled));
    map.insert("event_icon_path_object", this->eventIconPaths[Event::Group::Object]);
    map.insert("event_icon_path_warp", this->eventIconPaths[Event::Group::Warp]);
    map.insert("event_icon_path_coord", this->eventIconPaths[Event::Group::Coord]);
    map.insert("event_icon_path_bg", this->eventIconPaths[Event::Group::Bg]);
    map.insert("event_icon_path_heal", this->eventIconPaths[Event::Group::Heal]);
    for (auto i = this->pokemonIconPaths.cbegin(), end = this->pokemonIconPaths.cend(); i != end; i++){
        const QString path = i.value();
        if (!path.isEmpty()) map.insert("pokemon_icon_path/" + i.key(), path);
    }
    for (auto i = this->identifiers.cbegin(), end = this->identifiers.cend(); i != end; i++) {
        map.insert("ident/"+defaultIdentifiers.value(i.key()).first, i.value());
    }
    map.insert("collision_sheet_path", this->collisionSheetPath);
    map.insert("collision_sheet_width", QString::number(this->collisionSheetWidth));
    map.insert("collision_sheet_height", QString::number(this->collisionSheetHeight));
    QStringList warpBehaviorStrs;
    for (auto value : this->warpBehaviors)
        warpBehaviorStrs.append("0x" + QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
    map.insert("warp_behaviors", warpBehaviorStrs.join(","));

    return map;
}

void ProjectConfig::init() {
    QString dirName = QDir(this->projectDir).dirName().toLower();

    BaseGameVersion version = stringToBaseGameVersion(dirName);
    if (version != BaseGameVersion::none) {
        this->baseGameVersion = version;
        logInfo(QString("Auto-detected base_game_version as '%1'").arg(getBaseGameVersionString(version)));
    } else {
        QDialog dialog(nullptr, Qt::WindowTitleHint);
        dialog.setWindowTitle("Project Configuration");
        dialog.setWindowModality(Qt::NonModal);

        QFormLayout form(&dialog);

        QComboBox *baseGameVersionComboBox = new QComboBox();
        baseGameVersionComboBox->addItem("pokeruby", BaseGameVersion::pokeruby);
        baseGameVersionComboBox->addItem("pokefirered", BaseGameVersion::pokefirered);
        baseGameVersionComboBox->addItem("pokeemerald", BaseGameVersion::pokeemerald);
        form.addRow(new QLabel("Game Version"), baseGameVersionComboBox);

        // TODO: Add an 'Advanced' button to open the project settings window (with some settings disabled)

        QDialogButtonBox buttonBox(QDialogButtonBox::Ok, Qt::Horizontal, &dialog);
        QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        form.addRow(&buttonBox);

        if (dialog.exec() == QDialog::Accepted) {
            this->baseGameVersion = static_cast<BaseGameVersion>(baseGameVersionComboBox->currentData().toInt());
        } else {
            logWarn(QString("No base_game_version selected, using default '%1'").arg(getBaseGameVersionString(this->baseGameVersion)));
        }
    }
    this->setUnreadKeys(); // Initialize version-specific options
}

*/
