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
    Project(QWidget *parent = nullptr);
    ~Project();

    Project(const Project &) = delete;
    Project & operator = (const Project &) = delete;

    inline QWidget *parentWidget() const { return static_cast<QWidget *>(parent()); }

public:
    QString root;
    QStringList groupNames;
    QMap<QString, int> mapGroups;
    QList<QStringList> groupedMapNames;
    QStringList mapNames;
    QMap<QString, QString> mapConstantToMapName;
    QMap<QString, QString> mapNameToMapConstant;
    QMap<QString, QString> mapNameToLayoutId;
    QMap<QString, QString> mapNameToMapSectionName;
    QStringList mapLayoutsTable;
    QStringList mapLayoutsTableMaster;
    QString layoutsLabel;
    QMap<QString, MapLayout*> mapLayouts;
    QMap<QString, MapLayout*> mapLayoutsMaster;
    QMap<QString, QString> mapSecToMapHoverName;
    QMap<QString, int> mapSectionNameToValue;
    QMap<int, QString> mapSectionValueToName;
    QMap<QString, EventGraphics*> eventGraphicsMap;
    QMap<QString, int> gfxDefines;
    QString defaultSong;
    QStringList songNames;
    QStringList itemNames;
    QStringList flagNames;
    QStringList varNames;
    QStringList movementTypes;
    QStringList mapTypes;
    QStringList mapBattleScenes;
    QStringList weatherNames;
    QStringList coordEventWeatherNames;
    QStringList secretBaseIds;
    QStringList bgEventFacingDirections;
    QStringList trainerTypes;
    QStringList globalScriptLabels;
    QStringList healLocationNames;
    QSet<QString> healLocationNamesToDelete;
    QMap<QString, QMap<QString, uint16_t>> metatileLabelsMap;
    QMap<QString, uint16_t> unusedMetatileLabels;
    QMap<QString, uint32_t> metatileBehaviorMap;
    QMap<uint32_t, QString> metatileBehaviorMapInverse;
    QMap<QString, QString> facingDirections;
    ParseUtil parser;
    QFileSystemWatcher fileWatcher;
    QMap<QString, qint64> modifiedFileTimestamps;
    bool usingAsmTilesets;
    QString importExportPath;
    QSet<QString> disabledSettingsNames;
    QSet<QString> topLevelMapFields;
    int pokemonMinLevel;
    int pokemonMaxLevel;
    int maxEncounterRate;
    bool wildEncountersLoaded;

    void set_root(QString);

    void initSignals();

    void clearMapCache();
    void clearTilesetCache();
    void clearMapLayouts();
    void clearEventGraphics();

    bool sanityCheck();
    bool load();

    QMap<QString, Map*> mapCache;
    Map* loadMap(QString);
    Map* getMap(QString);

    QMap<QString, Tileset*> tilesetCache;
    Tileset* loadTileset(QString, Tileset *tileset = nullptr);
    Tileset* getTileset(QString, bool forceLoad = false);
    QStringList primaryTilesetLabels;
    QStringList secondaryTilesetLabels;
    QStringList tilesetLabelsOrdered;

    Blockdata readBlockdata(QString);
    bool loadBlockdata(MapLayout*);
    bool loadLayoutBorder(MapLayout*);

    void saveTextFile(QString path, QString text);
    void appendTextFile(QString path, QString text);
    void deleteFile(QString path);

    bool readMapGroups();
    Map* addNewMapToGroup(Map*, int, bool, bool);
    QString getNewMapName();
    QString getProjectTitle();

    bool readWildMonData();
    tsl::ordered_map<QString, tsl::ordered_map<QString, WildPokemonHeader>> wildMonData;

    QVector<EncounterField> wildMonFields;
    QVector<QString> encounterGroupLabels;
    QVector<poryjson::Json::object> extraEncounterGroups;

    bool readSpeciesIconPaths();
    QMap<QString, QString> speciesToIconPath;

    void initTopLevelMapFields();
    bool readMapJson(const QString &mapName, QJsonDocument * out);
    bool loadMapData(Map*);
    bool readMapLayouts();
    bool loadLayout(MapLayout *);
    bool loadMapLayout(Map*);
    bool loadLayoutTilesets(MapLayout*);
    void loadTilesetAssets(Tileset*);
    void loadTilesetTiles(Tileset*, QImage);
    void loadTilesetMetatiles(Tileset*);
    void loadTilesetMetatileLabels(Tileset*);
    void loadTilesetPalettes(Tileset*);
    void readTilesetPaths(Tileset* tileset);

    void saveLayoutBlockdata(Map*);
    void saveLayoutBorder(Map*);
    void writeBlockdata(QString, const Blockdata &);
    void saveAllMaps();
    void saveMap(Map*);
    void saveAllDataStructures();
    void saveConfig();
    void saveMapLayouts();
    void saveMapGroups();
    void saveWildMonData();
    void saveHealLocationsConstants();
    void saveTilesets(Tileset*, Tileset*);
    void saveTilesetMetatileLabels(Tileset*, Tileset*);
    void saveTilesetMetatileAttributes(Tileset*);
    void saveTilesetMetatiles(Tileset*);
    void saveTilesetTilesImage(Tileset*);
    void saveTilesetPalettes(Tileset*);
    void appendTilesetLabel(QString label, QString isSecondaryStr);
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
    bool readHealLocationConstants();
    bool readMiscellaneousConstants();
    bool readEventScriptLabels();
    bool readObjEventGfxConstants();
    bool readSongNames();
    bool readEventGraphics();
    bool readFieldmapProperties();
    bool readFieldmapMasks();
    QMap<QString, QMap<QString, QString>> readObjEventGfxInfo();

    void setEventPixmap(Event *event, bool forceLoad = false);

    QString fixPalettePath(QString path);
    QString fixGraphicPath(QString path);

    static QString getScriptFileExtension(bool usePoryScript);
    QString getScriptDefaultString(bool usePoryScript, QString mapName) const;
    QStringList getEventScriptsFilePaths() const;

    QString getDefaultPrimaryTilesetLabel();
    QString getDefaultSecondaryTilesetLabel();
    void updateTilesetMetatileLabels(Tileset *tileset);
    QString buildMetatileLabelsText(const QMap<QString, uint16_t> defines);
    QString findMetatileLabelsTileset(QString label);

    void setImportExportPath(QString filename);
    static QString getExistingFilepath(QString filepath);
    void applyParsedLimits();
    QString getDefaultHealLocationName(const QString& mapConstant);

    static QString getDynamicMapDefineName();
    static QString getDynamicMapName();
    static int getNumTilesPrimary();
    static int getNumTilesTotal();
    static int getNumMetatilesPrimary();
    static int getNumMetatilesTotal();
    static int getNumPalettesPrimary();
    static int getNumPalettesTotal();
    static int getMaxMapDataSize();
    static int getDefaultMapDimension();
    static int getMaxMapWidth();
    static int getMaxMapHeight();
    static int getMapDataSize(int width, int height);
    static bool mapDimensionsValid(int width, int height);
    bool calculateDefaultMapSize();
    static int getMaxObjectEvents();

private:
    void updateMapLayout(Map*);

    void setNewMapBlockdata(Map* map);
    void setNewMapBorder(Map *map);
    bool isHealLocationIdUnique(const QString &id);

    void ignoreWatchedFileTemporarily(QString filepath);

    static int num_tiles_primary;
    static int num_tiles_total;
    static int num_metatiles_primary;
    static int num_pals_primary;
    static int num_pals_total;
    static int max_map_data_size;
    static int default_map_dimension;
    static int max_object_events;

signals:
    void reloadProject();
    void uncheckMonitorFilesAction();
    void mapCacheCleared();
    void mapLoaded(Map *map);
};

#endif // PROJECT_H
