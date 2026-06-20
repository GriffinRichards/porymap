#pragma once
#ifndef LOGSTATUSBAR_H
#define LOGSTATUSBAR_H

#include "log.h"

#include <QLabel>
#include <QStatusBar>
#include <QTimer>

class LogStatusBar : public QStatusBar
{
    Q_OBJECT
public:
    LogStatusBar(QWidget* parent = nullptr);
    ~LogStatusBar();

    void showLogMessage(const QString& message, LogType type);
    void clearLogMessage();

    void setLogMessageTimeout(std::chrono::milliseconds interval) {m_timeout = interval;}
    std::chrono::milliseconds logMessageTimeout() const {return m_timeout;}

    void setLogTypes(const QSet<LogType>& logTypes) { m_acceptedLogTypes = logTypes;}
    const QSet<LogType>& logTypes() const {return m_acceptedLogTypes;}

private:
    QLabel m_icon;
    QLabel m_message;
    QTimer m_timer;
    std::chrono::milliseconds m_timeout{5000};
    QSet<LogType> m_acceptedLogTypes;
};

#endif // LOGSTATUSBAR_H
