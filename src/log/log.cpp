#include "log.h"
#include "logsignaler.h"
#include "logwriter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QSysInfo>
#include <QtDebug>
#include <QThread>

namespace Log {
    static QString mostRecentError;
    static QThread writeThread;
    static LogWriter writer;
}

// The basic log functions just emit a signal that a message has been logged.
// Other objects are free to respond to this event however they choose.
// Normally we only have two things responding to log signals:
// - The LogWriter, which formats the message and writes it disk / the console
// - The main window's LogStatusBar, which displays a selection of messages to the user.

void logInfo(const QString &message) {
    emit LogSignaler::instance()->logged(message, LogType::LOG_INFO);
}

void logWarn(const QString &message) {
    emit LogSignaler::instance()->logged(message, LogType::LOG_WARN);
}

void logError(const QString &message) {
    Log::mostRecentError = message;
    emit LogSignaler::instance()->logged(message, LogType::LOG_ERROR);
}

void logQt(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // If any part of our process for writing Qt log messages to disk causes an internal
    // Qt log event to occur we need to ignore it to prevent infinite recursion.
    thread_local bool ignoreQtLogs = false;
    if (ignoreQtLogs) return;
    ignoreQtLogs = true;

    emit LogSignaler::instance()->loggedQt(qFormatLogMessage(type, context, msg), type);

    ignoreQtLogs = false;
}

QString getLogPath() {
    return Log::writer.path();
}

QString getMostRecentError() {
    return Log::mostRecentError;
}

void logInit() {
    static bool initialized = false;
    if (initialized) return;

    const QString settingsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(settingsPath);
    if (!dir.exists()) dir.mkpath(settingsPath);

    Log::writer.setPath(dir.absoluteFilePath(QStringLiteral("porymap.log")));
    Log::writer.moveToThread(&Log::writeThread);
    QObject::connect(LogSignaler::instance(), &LogSignaler::logged, &Log::writer, &LogWriter::write);
    QObject::connect(LogSignaler::instance(), &LogSignaler::loggedQt, &Log::writer, &LogWriter::writeQt);
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, &Log::writeThread, [&] {
        Log::writeThread.quit();
        Log::writeThread.wait();
    });

    qInstallMessageHandler(logQt);
    Log::writeThread.start();

    initialized = true;
}
