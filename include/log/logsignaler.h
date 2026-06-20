#pragma once
#ifndef LOGSIGNALER_H
#define LOGSIGNALER_H

#include "log.h"

#include <QObject>

class LogSignaler : public QObject
{
    Q_OBJECT
public:
    Q_SIGNAL void logged(const QString& message, LogType type);
    Q_SIGNAL void loggedQt(const QString& message, QtMsgType type);
    static LogSignaler* instance();
};

#endif // LOGSIGNALER_H
