#pragma once
#ifndef PROJECT_H
#define PROJECT_H

#include "map.h"
#include "blockdata.h"
#include "wildmoninfo.h"
#include "parseutil.h"
#include "orderedjson.h"
#include "regionmap.h"

#include <QStringList>
#include <QList>
#include <QVector>
#include <QPair>
#include <QStandardItem>
#include <QVariant>
#include <QFileSystemWatcher>

class Project : public QObject
{
    Q_OBJECT
public:
    Project(QObject *parent = nullptr);
    ~Project();

    Project(const Project &) = delete;
    Project & operator = (const Project &) = delete;

    inline QWidget *parentWidget() const { return static_cast<QWidget *>(parent()); }

public:
    QString root;
    QStringList mapNames;
    QStringList groupNames;
    QMap<QString, QStringList> groupNameToMapNames;
    QStringList healLocationSaveOrder;
    QMap<QString, QList<HealLocationEvent*>> healLocations;
    QMap<QString, QString> mapConstantsToMapNames;
    QString layoutsLabel;
    QStringList layoutIds;
    QStringList layoutIdsMaster;
    QMap<QString, Layout*> mapLayouts;
    QMap<QString, Layout*> mapLayoutsMaster;
    QMap<QString, int> gfxDefines;
    QString defaultSong;
    QStringList songNames;
    QStringList itemNames;
    QStringList flagNames;
    QStringList varNames;
    QStringList speciesNames;
    QStringList movementTypes;
    QStringList mapTypes;
    QStringList mapBattleScenes;
    QStringList weatherNames;
    QStringList coordEventWeatherNames;
    QStringList secretBaseIds;
    QStringList bgEventFacingDirections;
    QStringList trainerTypes;
    QStringList globalScriptLabels;
    QStringList mapSectionIdNamesSaveOrder;
    QStringList mapSectionIdNames;
    QMap<uint32_t, QString> encounterTypeToName;
    QMap<uint32_t, QString> terrainTypeToName;
    QMap<QString, QMap<QString, uint16_t>> metatileLabelsMap;
    QMap<QString, uint16_t> unusedMetatileLabels;
    QMap<QString, uint32_t> metatileBehaviorMap;
    QMap<uint32_t, QString> metatileBehaviorMapInverse;
    ParseUtil parser;
    QFileSystemWatcher fileWatcher;
    QSet<QString> modifiedFiles;
    bool usingAsmTilesets;
    QSet<QString> disabledSettingsNames;
    int pokemonMinLevel;
    int pokemonMaxLevel;
    int maxEncounterRate;
    bool wildEncountersLoaded;

    void set_root(QString);

    void clearMaps();
    void clearTilesetCache();
    void clearMapLayouts();
    void clearEventGraphics();
    void clearHealLocations();

    bool sanityCheck();
    bool load();

    Map* loadMap(const QString &mapName);

    // Note: This does not guarantee the map is loaded.
    Map* getMap(const QString &mapName) { return this->maps.value(mapName); }

    bool isMapLoaded(const Map *map) const { return map && isMapLoaded(map->name()); }
    bool isMapLoaded(const QString &mapName) const { return this->loadedMapNames.contains(mapName); }
    bool isLayoutLoaded(const Layout *layout) const { return layout && isLayoutLoaded(layout->id); }
    bool isLayoutLoaded(const QString &layoutId) const { return this->loadedLayoutIds.contains(layoutId); }

    QMap<QString, Tileset*> tilesetCache;
    Tileset* loadTileset(QString, Tileset *tileset = nullptr);
    Tileset* getTileset(QString, bool forceLoad = false);
    QStringList primaryTilesetLabels;
    QStringList secondaryTilesetLabels;
    QStringList tilesetLabelsOrdered;

    bool readMapGroups();
    void addNewMapGroup(const QString &groupName);
    QString mapNameToMapGroup(const QString &mapName) const;
    QString getMapConstant(const QString &mapName, const QString &defaultValue = QString()) const;
    QString getMapLayoutId(const QString &mapName, const QString &defaultValue = QString()) const;
    QString getMapLocation(const QString &mapName, const QString &defaultValue = QString()) const;

    struct NewMapSettings {
        QString name;
        QString group;
        bool canFlyTo;
        Layout::Settings layout;
        MapHeader header;
    };
    NewMapSettings newMapSettings;
    Layout::Settings newLayoutSettings;

    void initNewMapSettings();
    void initNewLayoutSettings();

    Map *createNewMap(const Project::NewMapSettings &mapSettings, const Map* toDuplicate = nullptr);
    Layout *createNewLayout(const Layout::Settings &layoutSettings, const Layout* toDuplicate = nullptr);
    Tileset *createNewTileset(QString name, bool secondary, bool checkerboardFill);
    bool isIdentifierUnique(const QString &identifier) const;
    bool isValidNewIdentifier(const QString &identifier) const;
    QString toUniqueIdentifier(const QString &identifier) const;
    QString getProjectTitle() const;
    QString getNewHealLocationName(const Map* map) const;

    bool readWildMonData();
    OrderedMap<QString, OrderedMap<QString, WildPokemonHeader>> wildMonData;

    QString wildMonTableName;
    QVector<EncounterField> wildMonFields;
    QVector<QString> encounterGroupLabels;

    bool readSpeciesIconPaths();
    QString getDefaultSpeciesIconPath(const QString &species);
    QPixmap getSpeciesIcon(const QString &species);

    bool addNewMapsec(const QString &idName, const QString &displayName = QString());
    void removeMapsec(const QString &idName);
    QString getMapsecDisplayName(const QString &idName) const { return this->locationData.value(idName).displayName; }
    void setMapsecDisplayName(const QString &idName, const QString &displayName);

    bool hasUnsavedChanges();
    bool hasUnsavedDataChanges = false;

    bool readMapJson(const QString &mapName, QJsonDocument * out);
    bool loadMapEvent(Map *map, QJsonObject json, Event::Type defaultType = Event::Type::None);
    bool loadMapData(Map*);
    bool readMapLayouts();
    Layout *loadLayout(QString layoutId);
    bool loadLayout(Layout *);
    bool loadMapLayout(Map*);
    bool loadLayoutTilesets(Layout *);
    bool loadTilesetAssets(Tileset*);
    void loadTilesetMetatileLabels(Tileset*);
    void readTilesetPaths(Tileset* tileset);

    bool saveAll();
    bool saveGlobalData();
    bool saveConfig();
    bool saveLayout(Layout *layout);
    bool saveMap(Map *map, bool skipLayout = false);
    bool saveTextFile(const QString &path, const QString &text);
    bool saveRegionMapSections();
    bool saveTilesets(Tileset*, Tileset*);
    bool saveTilesetMetatileLabels(Tileset*, Tileset*);

    void appendTilesetLabel(const QString &label, const QString &isSecondaryStr);
    bool readTilesetLabels();
    bool readTilesetMetatileLabels();
    bool readRegionMapSections();
    bool readItemNames();
    bool readFlagNames();
    bool readVarNames();
    bool readMovementTypes();
    bool readInitialFacingDirections();
    bool readMapTypes();
    bool readMapBattleScenes();
    bool readWeatherNames();
    bool readCoordEventWeatherNames();
    bool readSecretBaseIds();
    bool readBgEventFacingDirections();
    bool readTrainerTypes();
    bool readMetatileBehaviors();
    bool readHealLocations();
    bool readMiscellaneousConstants();
    bool readEventScriptLabels();
    bool readObjEventGfxConstants();
    bool readSongNames();
    bool readEventGraphics();
    bool readFieldmapProperties();
    bool readFieldmapMasks();
    QMap<QString, QMap<QString, QString>> readObjEventGfxInfo();

    QPixmap getEventPixmap(const QString &gfxName, const QString &movementName);
    QPixmap getEventPixmap(const QString &gfxName, int frame, bool hFlip);
    QPixmap getEventPixmap(Event::Group group);
    void loadEventPixmap(Event *event, bool forceLoad = false);

    QString fixPalettePath(QString path);
    QString fixGraphicPath(QString path);

    static QString getScriptFileExtension(bool usePoryScript);
    QString getScriptDefaultString(bool usePoryScript, QString mapName) const;
    QStringList getEventScriptsFilePaths() const;
    void insertGlobalScriptLabels(QStringList &scriptLabels) const;

    QString getDefaultPrimaryTilesetLabel() const;
    QString getDefaultSecondaryTilesetLabel() const;
    void updateTilesetMetatileLabels(Tileset *tileset);
    QString buildMetatileLabelsText(const QMap<QString, uint16_t> defines);
    QString findMetatileLabelsTileset(QString label);

    static QString getExistingFilepath(QString filepath);
    void applyParsedLimits();
    
    void setRegionMapEntries(const QHash<QString, MapSectionEntry> &entries);
    QHash<QString, MapSectionEntry> getRegionMapEntries() const;

    QSet<QString> getTopLevelMapFields() const;

    int getMapDataSize(int width, int height) const;
    int getMaxMapDataSize() const { return this->maxMapDataSize; }
    int getMaxMapWidth() const;
    int getMaxMapHeight() const;
    bool mapDimensionsValid(int width, int height) const;
    bool calculateDefaultMapSize();
    QSize getDefaultMapSize() const { return this->defaultMapSize; }
    QSize getMapSizeAddition() const { return this->mapSizeAddition; }

    int getMaxEvents(Event::Group group) const;

    static QString getEmptyMapDefineName();
    static QString getDynamicMapDefineName();
    static QString getDynamicMapName();
    static QString getEmptySpeciesName();
    static QMargins getMetatileViewDistance();
    static int getNumTilesPrimary() { return num_tiles_primary; }
    static int getNumTilesTotal() { return num_tiles_total; }
    static int getNumMetatilesPrimary() { return num_metatiles_primary; }
    static int getNumMetatilesTotal() { return Block::getMaxMetatileId() + 1; }
    static int getNumPalettesPrimary(){ return num_pals_primary; }
    static int getNumPalettesTotal() { return num_pals_total; }
    static QString getEmptyMapsecName();
    static QString getMapGroupPrefix();

private:
    QMap<QString, qint64> modifiedFileTimestamps;
    QMap<QString, QString> facingDirections;
    QHash<QString, QString> speciesToIconPath;
    QHash<QString, Map*> maps;

    // Fields for preserving top-level JSON data that Porymap isn't expecting.
    QJsonObject customLayoutsData;
    QJsonObject customMapSectionsData;
    QJsonObject customMapGroupsData;
    QJsonObject customHealLocationsData;
    OrderedJson::object customWildMonData;
    OrderedJson::object customWildMonGroupData;
    OrderedJson::array extraEncounterGroups;

    // Maps/layouts represented in these sets have been fully loaded from the project.
    // If a valid map name / layout id is not in these sets, a Map / Layout object exists
    // for it in Project::maps / Project::mapLayouts, but it has been minimally populated
    // (i.e. for a map layout it only has the data read from layouts.json, none of its assets
    // have been loaded, and for a map it only has the data needed to identify it in the map
    // list, none of the rest of its data in map.json).
    QSet<QString> loadedMapNames;
    QSet<QString> loadedLayoutIds;

    // Data for layouts that failed to load at launch.
    // We can't display these layouts to the user, but we want to preserve the data when they save.
    QList<QJsonObject> failedLayoutsData;

    const QRegularExpression re_gbapalExtension;
    const QRegularExpression re_bppExtension;

    struct EventGraphics
    {
        QString filepath;
        bool loaded = false;
        QImage spritesheet;
        int spriteWidth = -1;
        int spriteHeight = -1;
        bool inanimate = false;
    };
    QMap<QString, EventGraphics*> eventGraphicsMap;

    // The extra data that can be associated with each MAPSEC name.
    struct LocationData
    {
        MapSectionEntry map;
        QString displayName;
        QJsonObject custom;
    };
    QHash<QString, LocationData> locationData;

    void setNewLayoutBlockdata(Layout *layout);
    void setNewLayoutBorder(Layout *layout);

    void ignoreWatchedFileTemporarily(QString filepath);
    void recordFileChange(const QString &filepath);
    void resetFileCache();

    bool saveMapLayouts();
    bool saveMapGroups();
    bool saveWildMonData();
    bool saveHealLocations();
    bool appendTextFile(const QString &path, const QString &text);

    QString findSpeciesIconPath(const QStringList &names) const;

    int maxObjectEvents;
    int maxMapDataSize;
    QSize defaultMapSize;
    QSize mapSizeAddition;

    // TODO: These really shouldn't be static, they're specific to a single project.
    //       We're making an assumption here that we only have one project open at a single time
    //       (which is true, but then if that's the case we should have some global Project instance instead)
    static int num_tiles_primary;
    static int num_tiles_total;
    static int num_metatiles_primary;
    static int num_pals_primary;
    static int num_pals_total;

signals:
    void fileChanged(const QString &filepath);
    void mapLoaded(Map *map);
    void mapCreated(Map *newMap, const QString &groupName);
    void layoutCreated(Layout *newLayout);
    void tilesetCreated(Tileset *newTileset);
    void mapGroupAdded(const QString &groupName);
    void mapSectionAdded(const QString &idName);
    void mapSectionDisplayNameChanged(const QString &idName, const QString &displayName);
    void mapSectionIdNamesChanged(const QStringList &idNames);
    void mapsExcluded(const QStringList &excludedMapNames);
    void eventScriptLabelsRead();
};

#endif // PROJECT_H
