#pragma once
#ifndef LOGWRITER_H
#define LOGWRITER_H

#include "log.h"

#include <QFile>
#include <QMutex>
#include <QObject>
#include <QTextStream>

class LogWriter : public QObject
{
    Q_OBJECT
public:
    LogWriter(QObject *parent = nullptr);
    LogWriter(const QString& path, QObject *parent = nullptr);
    ~LogWriter()=default;

    bool setPath(const QString& path);
    QString path() const;

    void setConsoleEnabled(bool consoleEnabled);
    bool consoleEnabled() const;

    Q_SLOT void write(const QString& message, LogType type);
    Q_SLOT void writeQt(const QString& message, QtMsgType type);


private:
    QFile m_file;
    QTextStream m_textStream;
    bool m_consoleEnabled{true};
    mutable QMutex m_mutex;

    template <typename T>
    void writeInternal(const QString& message, T type);
};

#endif // LOGWRITER_H
