#include "config.h"

UserConfig userConfig;
/*
QString UserConfig::getConfigFilepath() {
    // porymap config file is in the same directory as porymap itself.
    return QDir(this->projectDir).filePath("porymap.user.cfg");
}

void UserConfig::parseConfigKeyValue(QString key, QString value) {
    if (key == "recent_map") {
        this->recentMap = value;
    } else if (key == "use_encounter_json") {
        this->useEncounterJson = getConfigBool(key, value);
    } else if (key == "custom_scripts") {
        this->parseCustomScripts(value);
    } else {
        logInvalidKey(key);
    }
    readKeys.append(key);
}

QMap<QString, QString> UserConfig::getKeyValueMap() {
    QMap<QString, QString> map;
    map.insert("recent_map", this->recentMap);
    map.insert("use_encounter_json", QString::number(this->useEncounterJson));
    map.insert("custom_scripts", this->outputCustomScripts());
    return map;
}

void UserConfig::init() {
    QString dirName = QDir(this->projectDir).dirName().toLower();
    this->useEncounterJson = true;
    this->customScripts.clear();
}*/

// Inverse of UserConfig::parseCustomScripts
QString UserConfig::outputCustomScripts() {
    QStringList list;
    QMapIterator<QString, bool> i(this->customScripts);
    while (i.hasNext()) {
        i.next();
        list.append(QString("%1:%2").arg(i.key()).arg(i.value() ? "1" : "0"));
    }
    return list.join(",");
}

void UserConfig::setCustomScripts(QStringList scripts, QList<bool> enabled) {
    this->customScripts.clear();
    size_t size = qMin(scripts.length(), enabled.length());
    for (size_t i = 0; i < size; i++)
        this->customScripts.insert(scripts.at(i), enabled.at(i));
}

QStringList UserConfig::getCustomScriptPaths() {
    return this->customScripts.keys();
}

QList<bool> UserConfig::getCustomScriptsEnabled() {
    return this->customScripts.values();
}