#pragma once
#ifndef LOG_H
#define LOG_H

#include <QString>

enum class LogType {
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
};

void logInit();
void logInfo(const QString &message);
void logWarn(const QString &message);
void logError(const QString &message);

QString getLogPath();
QString getMostRecentError();

#endif // LOG_H
