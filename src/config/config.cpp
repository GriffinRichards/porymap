#include "config.h"
#include "log.h"
#include "map.h"
#include <QFile>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QList>
#include <QComboBox>
#include <QLabel>
#include <QTextStream>
#include <QRegularExpression>
#include <QAction>
#include <QAbstractButton>


KeyValueConfigBase::~KeyValueConfigBase() {

}

void KeyValueConfigBase::load() {
    //reset();
    QFile file(this->getConfigFilepath());
    if (!file.exists()) {
        this->init();
    } else if (!file.open(QIODevice::ReadOnly)) {
        logError(QString("Could not open config file '%1': ").arg(this->getConfigFilepath()) + file.errorString());
    }

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
            logWarn(QString("Invalid config line in %1: '%2'").arg(this->getConfigFilepath()).arg(line));
            continue;
        }

        this->parseConfigKeyValue(match.captured("key").trimmed().toLower(), match.captured("value").trimmed());
    }
    this->setUnreadKeys();

    file.close();
}

void KeyValueConfigBase::save() {
    QString text = "";
    QMap<QString, QString> map = this->getKeyValueMap();
    for (QMap<QString, QString>::iterator it = map.begin(); it != map.end(); it++) {
        text += QString("%1=%2\n").arg(it.key()).arg(it.value());
    }

    QFile file(this->getConfigFilepath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(text.toUtf8());
        file.close();
    } else {
        logError(QString("Could not open config file '%1' for writing: ").arg(this->getConfigFilepath()) + file.errorString());
    }
}

int KeyValueConfigBase::getConfigInteger(QString key, QString value, int min, int max, int defaultValue) {
    bool ok;
    int result = value.toInt(&ok, 0);
    if (!ok) {
        logWarn(QString("Invalid config value for %1: '%2'. Must be an integer.").arg(key).arg(value));
        return defaultValue;
    }
    return qMin(max, qMax(min, result));
}

uint32_t KeyValueConfigBase::getConfigUint32(QString key, QString value, uint32_t min, uint32_t max, uint32_t defaultValue) {
    bool ok;
    uint32_t result = value.toUInt(&ok, 0);
    if (!ok) {
        logWarn(QString("Invalid config value for %1: '%2'. Must be an integer.").arg(key).arg(value));
        return defaultValue;
    }
    return qMin(max, qMax(min, result));
}

void KeyValueConfigBase::logInvalidKey(const QString &key) {
    logWarn(QString("Invalid config key found in config file %1: '%2'").arg(this->getConfigFilepath()).arg(key));
}

