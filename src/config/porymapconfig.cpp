#include "config.h"

PorymapConfig porymapConfig;

const QMap<MapSortOrder, QString> mapSortOrderMap = {
    {MapSortOrder::Group, "group"},
    {MapSortOrder::Layout, "layout"},
    {MapSortOrder::Area, "area"},
};

const QMap<QString, MapSortOrder> mapSortOrderReverseMap = {
    {"group", MapSortOrder::Group},
    {"layout", MapSortOrder::Layout},
    {"area", MapSortOrder::Area},
};
/*
QString PorymapConfig::getConfigFilepath() {
    // porymap config file is in the same directory as porymap itself.
    QString settingsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(settingsPath);
    if (!dir.exists())
        dir.mkpath(settingsPath);

    QString configPath = dir.absoluteFilePath("porymap.cfg");

    return configPath;
}
*/

/*
void PorymapConfig::parseConfigKeyValue(QString key, QString value) {
    if (key == "recent_project") {
        this->recentProjects = value.split(",", Qt::SkipEmptyParts);
        this->recentProjects.removeDuplicates();
    } else if (key == "project_manually_closed") {
        this->projectManuallyClosed = getConfigBool(key, value);
    } else if (key == "reopen_on_launch") {
        this->reopenOnLaunch = getConfigBool(key, value);
    } else if (key == "pretty_cursors") {
        this->prettyCursors = getConfigBool(key, value);
    } else if (key == "map_sort_order") {
        QString sortOrder = value.toLower();
        if (mapSortOrderReverseMap.contains(sortOrder)) {
            this->mapSortOrder = mapSortOrderReverseMap.value(sortOrder);
        } else {
            this->mapSortOrder = MapSortOrder::Group;
            logWarn(QString("Invalid config value for map_sort_order: '%1'. Must be 'group', 'area', or 'layout'.").arg(value));
        }
    } else if (key == "main_window_geometry") {
        this->mainWindowGeometry = bytesFromString(value);
    } else if (key == "main_window_state") {
        this->mainWindowState = bytesFromString(value);
    } else if (key == "map_splitter_state") {
        this->mapSplitterState = bytesFromString(value);
    } else if (key == "main_splitter_state") {
        this->mainSplitterState = bytesFromString(value);
    } else if (key == "metatiles_splitter_state") {
        this->metatilesSplitterState = bytesFromString(value);
    } else if (key == "mirror_connecting_maps") {
        this->mirrorConnectingMaps = getConfigBool(key, value);
    } else if (key == "show_dive_emerge_maps") {
        this->showDiveEmergeMaps = getConfigBool(key, value);
    } else if (key == "dive_emerge_map_opacity") {
        this->diveEmergeMapOpacity = getConfigInteger(key, value, 10, 90, 30);
    } else if (key == "dive_map_opacity") {
        this->diveMapOpacity = getConfigInteger(key, value, 10, 90, 15);
    } else if (key == "emerge_map_opacity") {
        this->emergeMapOpacity = getConfigInteger(key, value, 10, 90, 15);
    } else if (key == "collision_opacity") {
        this->collisionOpacity = getConfigInteger(key, value, 0, 100, 50);
    } else if (key == "tileset_editor_geometry") {
        this->tilesetEditorGeometry = bytesFromString(value);
    } else if (key == "tileset_editor_state") {
        this->tilesetEditorState = bytesFromString(value);
    } else if (key == "tileset_editor_splitter_state") {
        this->tilesetEditorSplitterState = bytesFromString(value);
    } else if (key == "palette_editor_geometry") {
        this->paletteEditorGeometry = bytesFromString(value);
    } else if (key == "palette_editor_state") {
        this->paletteEditorState = bytesFromString(value);
    } else if (key == "region_map_editor_geometry") {
        this->regionMapEditorGeometry = bytesFromString(value);
    } else if (key == "region_map_editor_state") {
        this->regionMapEditorState = bytesFromString(value);
    } else if (key == "project_settings_editor_geometry") {
        this->projectSettingsEditorGeometry = bytesFromString(value);
    } else if (key == "project_settings_editor_state") {
        this->projectSettingsEditorState = bytesFromString(value);
    } else if (key == "custom_scripts_editor_geometry") {
        this->customScriptsEditorGeometry = bytesFromString(value);
    } else if (key == "custom_scripts_editor_state") {
        this->customScriptsEditorState = bytesFromString(value);
    } else if (key == "wild_mon_chart_geometry") {
        this->wildMonChartGeometry = bytesFromString(value);
    } else if (key == "metatiles_zoom") {
        this->metatilesZoom = getConfigInteger(key, value, 10, 100, 30);
    } else if (key == "collision_zoom") {
        this->collisionZoom = getConfigInteger(key, value, 10, 100, 30);
    } else if (key == "tileset_editor_metatiles_zoom") {
        this->tilesetEditorMetatilesZoom = getConfigInteger(key, value, 10, 100, 30);
    } else if (key == "tileset_editor_tiles_zoom") {
        this->tilesetEditorTilesZoom = getConfigInteger(key, value, 10, 100, 30);
    } else if (key == "show_player_view") {
        this->showPlayerView = getConfigBool(key, value);
    } else if (key == "show_cursor_tile") {
        this->showCursorTile = getConfigBool(key, value);
    } else if (key == "show_border") {
        this->showBorder = getConfigBool(key, value);
    } else if (key == "show_grid") {
        this->showGrid = getConfigBool(key, value);
    } else if (key == "show_tileset_editor_metatile_grid") {
        this->showTilesetEditorMetatileGrid = getConfigBool(key, value);
    } else if (key == "show_tileset_editor_layer_grid") {
        this->showTilesetEditorLayerGrid = getConfigBool(key, value);
    } else if (key == "monitor_files") {
        this->monitorFiles = getConfigBool(key, value);
    } else if (key == "tileset_checkerboard_fill") {
        this->tilesetCheckerboardFill = getConfigBool(key, value);
    } else if (key == "theme") {
        this->theme = value;
    } else if (key == "wild_mon_chart_theme") {
        this->wildMonChartTheme = value;
    } else if (key == "text_editor_open_directory") {
        this->textEditorOpenFolder = value;
    } else if (key == "text_editor_goto_line") {
        this->textEditorGotoLine = value;
    } else if (key == "palette_editor_bit_depth") {
        this->paletteEditorBitDepth = getConfigInteger(key, value, 15, 24, 24);
        if (this->paletteEditorBitDepth != 15 && this->paletteEditorBitDepth != 24){
            this->paletteEditorBitDepth = 24;
        }
    } else if (key == "project_settings_tab") {
        this->projectSettingsTab = getConfigInteger(key, value, 0);
    } else if (key == "warp_behavior_warning_disabled") {
        this->warpBehaviorWarningDisabled = getConfigBool(key, value);
    } else if (key == "check_for_updates") {
        this->checkForUpdates = getConfigBool(key, value);
    } else if (key == "last_update_check_time") {
        this->lastUpdateCheckTime = QDateTime::fromString(value).toLocalTime();
    } else if (key == "last_update_check_version") {
        auto version = QVersionNumber::fromString(value);
        if (version.segmentCount() != 3) {
            logWarn(QString("Invalid config value for %1: '%2'. Must be 3 numbers separated by '.'").arg(key).arg(value));
            this->lastUpdateCheckVersion = porymapVersion;
        } else {
            this->lastUpdateCheckVersion = version;
        }
    } else if (key.startsWith("rate_limit_time/")) {
        static const QRegularExpression regex("\\brate_limit_time/(?<url>.+)");
        QRegularExpressionMatch match = regex.match(key);
        if (match.hasMatch()) {
            this->rateLimitTimes.insert(match.captured("url"), QDateTime::fromString(value).toLocalTime());
        }
    } else {
        logInvalidKey(key);
    }
}


QMap<QString, QString> PorymapConfig::getKeyValueMap() {
    QMap<QString, QString> map;
    map.insert("recent_project", this->recentProjects.join(","));
    map.insert("project_manually_closed", this->projectManuallyClosed ? "1" : "0");
    map.insert("reopen_on_launch", this->reopenOnLaunch ? "1" : "0");
    map.insert("pretty_cursors", this->prettyCursors ? "1" : "0");
    map.insert("map_sort_order", mapSortOrderMap.value(this->mapSortOrder));
    map.insert("main_window_geometry", stringFromByteArray(this->mainWindowGeometry));
    map.insert("main_window_state", stringFromByteArray(this->mainWindowState));
    map.insert("map_splitter_state", stringFromByteArray(this->mapSplitterState));
    map.insert("main_splitter_state", stringFromByteArray(this->mainSplitterState));
    map.insert("metatiles_splitter_state", stringFromByteArray(this->metatilesSplitterState));
    map.insert("tileset_editor_geometry", stringFromByteArray(this->tilesetEditorGeometry));
    map.insert("tileset_editor_state", stringFromByteArray(this->tilesetEditorState));
    map.insert("tileset_editor_splitter_state", stringFromByteArray(this->tilesetEditorSplitterState));
    map.insert("palette_editor_geometry", stringFromByteArray(this->paletteEditorGeometry));
    map.insert("palette_editor_state", stringFromByteArray(this->paletteEditorState));
    map.insert("region_map_editor_geometry", stringFromByteArray(this->regionMapEditorGeometry));
    map.insert("region_map_editor_state", stringFromByteArray(this->regionMapEditorState));
    map.insert("project_settings_editor_geometry", stringFromByteArray(this->projectSettingsEditorGeometry));
    map.insert("project_settings_editor_state", stringFromByteArray(this->projectSettingsEditorState));
    map.insert("custom_scripts_editor_geometry", stringFromByteArray(this->customScriptsEditorGeometry));
    map.insert("custom_scripts_editor_state", stringFromByteArray(this->customScriptsEditorState));
    map.insert("wild_mon_chart_geometry", stringFromByteArray(this->wildMonChartGeometry));
    map.insert("mirror_connecting_maps", this->mirrorConnectingMaps ? "1" : "0");
    map.insert("show_dive_emerge_maps", this->showDiveEmergeMaps ? "1" : "0");
    map.insert("dive_emerge_map_opacity", QString::number(this->diveEmergeMapOpacity));
    map.insert("dive_map_opacity", QString::number(this->diveMapOpacity));
    map.insert("emerge_map_opacity", QString::number(this->emergeMapOpacity));
    map.insert("collision_opacity", QString::number(this->collisionOpacity));
    map.insert("collision_zoom", QString::number(this->collisionZoom));
    map.insert("metatiles_zoom", QString::number(this->metatilesZoom));
    map.insert("tileset_editor_metatiles_zoom", QString::number(this->tilesetEditorMetatilesZoom));
    map.insert("tileset_editor_tiles_zoom", QString::number(this->tilesetEditorTilesZoom));
    map.insert("show_player_view", this->showPlayerView ? "1" : "0");
    map.insert("show_cursor_tile", this->showCursorTile ? "1" : "0");
    map.insert("show_border", this->showBorder ? "1" : "0");
    map.insert("show_grid", this->showGrid ? "1" : "0");
    map.insert("show_tileset_editor_metatile_grid", this->showTilesetEditorMetatileGrid ? "1" : "0");
    map.insert("show_tileset_editor_layer_grid", this->showTilesetEditorLayerGrid ? "1" : "0");
    map.insert("monitor_files", this->monitorFiles ? "1" : "0");
    map.insert("tileset_checkerboard_fill", this->tilesetCheckerboardFill ? "1" : "0");
    map.insert("theme", this->theme);
    map.insert("wild_mon_chart_theme", this->wildMonChartTheme);
    map.insert("text_editor_open_directory", this->textEditorOpenFolder);
    map.insert("text_editor_goto_line", this->textEditorGotoLine);
    map.insert("palette_editor_bit_depth", QString::number(this->paletteEditorBitDepth));
    map.insert("project_settings_tab", QString::number(this->projectSettingsTab));
    map.insert("warp_behavior_warning_disabled", QString::number(this->warpBehaviorWarningDisabled));
    map.insert("check_for_updates", QString::number(this->checkForUpdates));
    map.insert("last_update_check_time", this->lastUpdateCheckTime.toUTC().toString());
    map.insert("last_update_check_version", this->lastUpdateCheckVersion.toString());
    for (auto i = this->rateLimitTimes.cbegin(), end = this->rateLimitTimes.cend(); i != end; i++){
        // Only include rate limit times that are still active (i.e., in the future)
        const QDateTime time = i.value();
        if (!time.isNull() && time > QDateTime::currentDateTime())
            map.insert("rate_limit_time/" + i.key().toString(), time.toUTC().toString());
    }
    
    return map;
}
*/

QString PorymapConfig::stringFromByteArray(QByteArray bytearray) {
    QString ret;
    for (auto ch : bytearray) {
        ret += QString::number(static_cast<int>(ch)) + ":";
    }
    return ret;
}

void PorymapConfig::addRecentProject(QString project) {
    this->recentProjects.removeOne(project);
    this->recentProjects.prepend(project);
}

void PorymapConfig::setRecentProjects(QStringList projects) {
    this->recentProjects = projects;
}

QString PorymapConfig::getRecentProject() {
    return this->recentProjects.value(0);
}

QStringList PorymapConfig::getRecentProjects() {
    return this->recentProjects;
}

void PorymapConfig::setMainGeometry(QByteArray mainWindowGeometry_, QByteArray mainWindowState_,
                                QByteArray mapSplitterState_, QByteArray mainSplitterState_, QByteArray metatilesSplitterState_) {
    this->mainWindowGeometry = mainWindowGeometry_;
    this->mainWindowState = mainWindowState_;
    this->mapSplitterState = mapSplitterState_;
    this->mainSplitterState = mainSplitterState_;
    this->metatilesSplitterState = metatilesSplitterState_;
}

void PorymapConfig::setTilesetEditorGeometry(QByteArray tilesetEditorGeometry_, QByteArray tilesetEditorState_,
                                            QByteArray tilesetEditorSplitterState_) {
    this->tilesetEditorGeometry = tilesetEditorGeometry_;
    this->tilesetEditorState = tilesetEditorState_;
    this->tilesetEditorSplitterState = tilesetEditorSplitterState_;
}

void PorymapConfig::setPaletteEditorGeometry(QByteArray paletteEditorGeometry_, QByteArray paletteEditorState_) {
    this->paletteEditorGeometry = paletteEditorGeometry_;
    this->paletteEditorState = paletteEditorState_;
}

void PorymapConfig::setRegionMapEditorGeometry(QByteArray regionMapEditorGeometry_, QByteArray regionMapEditorState_) {
    this->regionMapEditorGeometry = regionMapEditorGeometry_;
    this->regionMapEditorState = regionMapEditorState_;
}

void PorymapConfig::setProjectSettingsEditorGeometry(QByteArray projectSettingsEditorGeometry_, QByteArray projectSettingsEditorState_) {
    this->projectSettingsEditorGeometry = projectSettingsEditorGeometry_;
    this->projectSettingsEditorState = projectSettingsEditorState_;
}

void PorymapConfig::setCustomScriptsEditorGeometry(QByteArray customScriptsEditorGeometry_, QByteArray customScriptsEditorState_) {
    this->customScriptsEditorGeometry = customScriptsEditorGeometry_;
    this->customScriptsEditorState = customScriptsEditorState_;
}

QMap<QString, QByteArray> PorymapConfig::getMainGeometry() {
    QMap<QString, QByteArray> geometry;

    geometry.insert("main_window_geometry", this->mainWindowGeometry);
    geometry.insert("main_window_state", this->mainWindowState);
    geometry.insert("map_splitter_state", this->mapSplitterState);
    geometry.insert("main_splitter_state", this->mainSplitterState);
    geometry.insert("metatiles_splitter_state", this->metatilesSplitterState);

    return geometry;
}

QMap<QString, QByteArray> PorymapConfig::getTilesetEditorGeometry() {
    QMap<QString, QByteArray> geometry;

    geometry.insert("tileset_editor_geometry", this->tilesetEditorGeometry);
    geometry.insert("tileset_editor_state", this->tilesetEditorState);
    geometry.insert("tileset_editor_splitter_state", this->tilesetEditorSplitterState);

    return geometry;
}

QMap<QString, QByteArray> PorymapConfig::getPaletteEditorGeometry() {
    QMap<QString, QByteArray> geometry;

    geometry.insert("palette_editor_geometry", this->paletteEditorGeometry);
    geometry.insert("palette_editor_state", this->paletteEditorState);

    return geometry;
}

QMap<QString, QByteArray> PorymapConfig::getRegionMapEditorGeometry() {
    QMap<QString, QByteArray> geometry;

    geometry.insert("region_map_editor_geometry", this->regionMapEditorGeometry);
    geometry.insert("region_map_editor_state", this->regionMapEditorState);

    return geometry;
}

QMap<QString, QByteArray> PorymapConfig::getProjectSettingsEditorGeometry() {
    QMap<QString, QByteArray> geometry;

    geometry.insert("project_settings_editor_geometry", this->projectSettingsEditorGeometry);
    geometry.insert("project_settings_editor_state", this->projectSettingsEditorState);

    return geometry;
}

QMap<QString, QByteArray> PorymapConfig::getCustomScriptsEditorGeometry() {
    QMap<QString, QByteArray> geometry;

    geometry.insert("custom_scripts_editor_geometry", this->customScriptsEditorGeometry);
    geometry.insert("custom_scripts_editor_state", this->customScriptsEditorState);

    return geometry;
}