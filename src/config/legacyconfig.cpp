#include "config.h"

#include <QRegularExpression>

/*
    Our config data used to be stored in a custom text format in .cfg files.
    We now store this data in .json files. If the user has an old .cfg file we use this
    function to read the data, and we will replace it with a new .json file.

    There shouldn't much of a reason to update this file. New fields added to the config
    won't be present in the old config format, and so won't need to be handled below.
*/

const QSet<QString> boolKeys = {
    "reopen_on_launch",
    "pretty_cursors",
    "show_player_view",
    "show_cursor_tile",
    "show_border",
    "show_grid",
    "show_tileset_editor_metatile_grid",
    "show_tileset_editor_layer_grid",
    "monitor_files",
    "tileset_checkerboard_fill",
    "warp_behavior_warning_disabled",
    "check_for_updates",
    "use_poryscript",
    "use_custom_border_size",
    "enable_event_weather_trigger",
    "enable_event_secret_base",
    "enable_hidden_item_quantity",
    "enable_hidden_item_requires_itemfinder",
    "enable_heal_location_respawn_data"
    "enable_event_clone_object",
    "enable_floor_number",
    "create_map_text_file",
    "enable_triple_layer_metatiles",
    "enable_map_allow_flags",
    "use_encounter_json",
    "prefabs_import_prompted",
    "tilesets_have_callback",
    "tilesets_have_is_compressed",
};

const QSet<QString> numberKeys = {
    "collisionOpacity",
    "metatiles_zoom",
    "collision_zoom",
    "tileset_editor_metatiles_zoom",
    "tileset_editor_tiles_zoom",
    "palette_editor_bit_depth",
    "project_settings_tab",
    "metatile_attributes_size",
    "default_metatile",
    "default_elevation",
    "default_collision",
    "metatile_behavior_mask",
    "metatile_terrain_type_mask",
    "metatile_encounter_type_mask",
    "metatile_layer_type_mask",
    "block_metatile_id_mask",
    "block_collision_mask",
    "block_elevation_mask",
    "collision_sheet_width",
    "collision_sheet_height",
};

const QSet<QString> colonSeparatedNumberKeys = {
    "main_window_geometry",
    "main_window_state",
    "map_splitter_state",
    "main_splitter_state",
    "metatiles_splitter_state",
    "tileset_editor_geometry",
    "tileset_editor_state",
    "tileset_editor_splitter_state",
    "palette_editor_geometry",
    "palette_editor_state",
    "region_map_editor_geometry",
    "region_map_editor_state",
    "project_settings_editor_geometry",
    "project_settings_editor_state",
    "custom_scripts_editor_geometry",
    "custom_scripts_editor_state",
};

const QSet<QString> commaSeparatedNumberKeys = {
    "new_map_border_metatiles",
    "warp_behaviors",
};

const QSet<QString> commaSeparatedStringKeys = {
    "recent_project"
};

// Some keys in the .cfg are grouped into categories using a prefix/name=value format.
// In the JSON we'll convert these to 'objectName = { name: "value", ... }'.
// We map the old prefix names to the new object names below
const QMap<QString,QString> keyPrefixMap = {
    {"rate_limit_time/", "rate_limit_times"},
    {"pokemon_icon_path/", "pokemon_icon_paths"},
    {"path/", "path_overrides"},
    {"ident/", "identifier_overrides"},
};

QJsonArray cfgNumberArrayToJson(QString input, QString delimiter) {
    auto arr = QJsonArray();
    input.remove(" ");
    for (auto s : input.split(delimiter, Qt::SkipEmptyParts)) {
        bool ok;
        auto num = input.toLongLong(&ok, 0);
        if (ok)
            arr.append(num);
    }
    return arr;
}

QJsonArray cfgStringArrayToJson(QString input, QString delimiter) {
    auto arr = QJsonArray();
    for (auto s : input.split(delimiter))
        arr.append(s);
    return arr;
}

QString getKeySpecialPrefix(const QString &key) {
    // Map old prefix names to new object group names
    static const auto prefixes = keyPrefixMap.keys();
    for (const auto prefix : prefixes) {
        if (key.startsWith(prefix))
            return prefix;
    }
    return QString();
}

QJsonDocument Config::fromCfg(const QString &filepath) {
    QFile file(filepath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return QJsonDocument();

    QJsonObject body = QJsonObject();
    QTextStream in(&file);
    QList<QString> configLines;
    static const QRegularExpression re("^(?<key>[^=]+)=(?<value>.*)$");
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        int commentIndex = line.indexOf("#");
        if (commentIndex >= 0) {
            line = line.left(commentIndex).trimmed();
        }

        if (line.length() == 0) {
            continue;
        }

        QRegularExpressionMatch match = re.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        QString key = match.captured("key").trimmed().toLower();
        QString value = match.captured("value").trimmed();

        // This key name changed between versions
        if (key == "enable_object_event_in_connection")
            key = "enable_event_clone_object";

        // Convert the .cfg key=value format to JSON
        if (key == "custom_scripts") {
            // .cfg format: comma separated list of paths that can have an 'enabled' number suffix.
            // JSON format: an array of objects with a string "path" and bool "disabled" property
            auto arr = QJsonArray();
            QStringList paths = value.split(",", Qt::SkipEmptyParts);
            for (auto path : paths) {
                bool disabled = path.endsWith(":0");
                if (disabled || path.endsWith(":1"))
                    path.chop(2);
                if (!path.isEmpty()) {
                    auto elem = QJsonObject();
                    elem["path"] = path;
                    elem["disabled"] = disabled;
                    arr.append(elem);
                }
            }
            if (!arr.isEmpty())
                body[key] = arr;
        } else if (boolKeys.contains(key)) {
            // .cfg format: key=0 or key=1
            // JSON format: boolean
            bool ok;
            auto num = value.toInt(&ok, 0);
            if (ok && (num == 0 || num == 1))
                body[key] = (num == 1);
        } else if (numberKeys.contains(key)) {
            // .cfg format: key=<number>
            // JSON format: number
            bool ok;
            auto num = value.toLongLong(&ok, 0);
            if (ok)
                body[key] = num;
        } else if (colonSeparatedNumberKeys.contains(key)) {
            // .cfg format: colon-separated list of numbers
            // JSON format: array of numbers
            auto arr = cfgNumberArrayToJson(value, ":");
            if (!arr.isEmpty())
                body[key] = arr;
        } else if (commaSeparatedNumberKeys.contains(key)) {
            // .cfg format: comma-separated list of numbers
            // JSON format: array of numbers
            auto arr = cfgNumberArrayToJson(value, ",");
            if (!arr.isEmpty())
                body[key] = arr;
        } else if (commaSeparatedStringKeys.contains(key)) {
            // .cfg format: comma-separated list of strings
            // JSON format: array of strings
            auto arr = cfgStringArrayToJson(value, ",");
            if (!arr.isEmpty())
                body[key] = arr;
        } else {
            const QString prefix = getKeySpecialPrefix(key);
            if (prefix.length() && key.length() > prefix.length()) {
                // .cfg format: prefix/name=value
                // JSON format: 'name: "value"', grouped together with other keys with the same prefix under a new object.
                key.remove(0, prefix.length());
                const QString newObjectName = keyPrefixMap.value(prefix);
                if (!body.contains(newObjectName))
                    body[newObjectName] = QJsonObject();

                // Qt (as of writing) doesn't have a clean way to edit nested QJsonObjects. We overwrite the outer object here.
                auto obj = body[newObjectName].toObject();
                obj[key] = value;
                body[newObjectName] = obj;
            } else {
                // Any remaining keys will have their values assigned plainly as strings
                body[key] = QJsonValue(value);
            }
        }

        // The old .cfg format listed most (but not all) of the settings, regardless of whether they had changed.
        // Our new JSON format only lists settings if the user has changed them from the default setting.
        // As part of the conversion we'll do a quick one-time pruning of any settings that have default values.
        // TODO (or actually, handle this when this function gets called)
    }
    return QJsonDocument(body);
}


//TODO ShortcutsConfig
