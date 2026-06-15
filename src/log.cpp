#include "log.h"
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QSysInfo>
#include <QLabel>
#include <QPointer>
#include <QTimer>

#include <iostream>

namespace Log {
    static QString mostRecentError;
    static QString path;
    static QFile file;
    static QTextStream textStream;
    static bool initialized = false;

    struct Display {
        QPointer<QStatusBar> statusBar;
        QPointer<QLabel> message;
        QPointer<QLabel> icon;
        QSet<LogType> acceptedTypes;
    };
    static QList<Display> displays;
    static QTimer displayClearTimer;
};

// Enabling this does not seem to be simple to color console output
// on Windows for all CLIs without external libraries or extreme bloat.
#ifdef Q_OS_WIN
    #define ERROR_COLOR   ""
    #define WARNING_COLOR ""
    #define INFO_COLOR    ""
    #define CLEAR_COLOR   ""
#else
    #define ERROR_COLOR   "\033[31;1m"
    #define WARNING_COLOR "\033[1;33m"
    #define INFO_COLOR    "\033[32m"
    #define CLEAR_COLOR   "\033[0m"
#endif

QString colorizeTypeName(const QString &message, const QString &typeName, const QString &colorFormat) {
    QString colorized = message;
    return colorized.replace(typeName, colorFormat + typeName + CLEAR_COLOR);
}

QString colorizeMessage(const QString &message, LogType type) {
    switch (type) {
    case LogType::LOG_INFO:  return colorizeTypeName(message, "INFO", INFO_COLOR);
    case LogType::LOG_WARN:  return colorizeTypeName(message, "WARN", WARNING_COLOR);
    case LogType::LOG_ERROR: return colorizeTypeName(message, "ERROR", ERROR_COLOR);
    }
}

QString colorizeMessage(const QString &message, QtMsgType type) {
    switch (type) {
    case QtDebugMsg:    return colorizeTypeName(message, "QT DEBUG", INFO_COLOR);
    case QtInfoMsg:     return colorizeTypeName(message, "QT INFO", INFO_COLOR);
    case QtWarningMsg:  return colorizeTypeName(message, "QT WARN", WARNING_COLOR);
    case QtCriticalMsg: return colorizeTypeName(message, "QT ERROR", ERROR_COLOR);
    case QtFatalMsg:    return colorizeTypeName(message, "QT FATAL", ERROR_COLOR);
    }
}

std::ostream& stream(LogType type) {
    switch (type) {
    case LogType::LOG_INFO:
        return std::cout;
    case LogType::LOG_WARN:
    case LogType::LOG_ERROR:
        return std::cerr;
    }
}

std::ostream& stream(QtMsgType type) {
    switch (type) {
    case QtDebugMsg:
    case QtInfoMsg:
        return std::cout;
    case QtWarningMsg:
    case QtCriticalMsg:
    case QtFatalMsg:
        return std::cerr;
    }
}

QString prefix(LogType type) {
    switch (type) {
    case LogType::LOG_INFO:  return QStringLiteral("    [INFO]");
    case LogType::LOG_WARN:  return QStringLiteral("    [WARN]");
    case LogType::LOG_ERROR: return QStringLiteral("   [ERROR]");
    }
}

QString prefix(QtMsgType type) {
    switch (type) {
    case QtDebugMsg:         return QStringLiteral("[QT DEBUG]");
    case QtInfoMsg:          return QStringLiteral(" [QT INFO]");
    case QtWarningMsg:       return QStringLiteral(" [QT WARN]");
    case QtCriticalMsg:      return QStringLiteral("[QT ERROR]");
    case QtFatalMsg:         return QStringLiteral("[QT FATAL]");
    }
}

void addLogStatusBar(QStatusBar *statusBar, const QSet<LogType> &acceptedTypes) {
    if (!statusBar) return;

    static const QSet<LogType> allTypes = {LOG_ERROR, LOG_WARN, LOG_INFO};

    Log::Display display = {
        .statusBar = statusBar,
        .message = new QLabel(statusBar),
        .icon = new QLabel(statusBar),
        .acceptedTypes = acceptedTypes.isEmpty() ? allTypes : acceptedTypes,
    };
    statusBar->addWidget(display.icon);
    statusBar->addWidget(display.message);
    Log::displays.append(display);
}

void removeLogStatusBar(int index) {
    Log::Display display = Log::displays.takeAt(index);
    display.statusBar->removeWidget(display.icon);
    display.statusBar->removeWidget(display.message);
    delete display.icon;
    delete display.message;
}

bool removeLogStatusBar(QStatusBar *statusBar) {
    if (!statusBar) return false;

    for (int i = 0; i < Log::displays.length(); i++) {
        if (Log::displays.at(i).statusBar == statusBar) {
            removeLogStatusBar(i);
            return true;
        }
    }
    return false;
}

void pruneLogDisplays() {
    auto it = QMutableListIterator<Log::Display>(Log::displays);
    while (it.hasNext()) {
        auto display = it.next();
        if (!display.statusBar) {
            // Status bar was deleted externally, remove entry from the list.
            it.remove();
        }
    }
}

void updateLogDisplays(const QString &/*message*/, QtMsgType /*type*/) {
    // Don't send Qt messages to status bar
}

void updateLogDisplays(const QString &message, LogType type) {
    static const QMap<LogType, QPixmap> icons = {
        {LogType::LOG_INFO,  QPixmap(QStringLiteral(":/icons/information.ico"))},
        {LogType::LOG_WARN,  QPixmap(QStringLiteral(":/icons/warning.ico"))},
        {LogType::LOG_ERROR, QPixmap(QStringLiteral(":/icons/error.ico"))},
    };

    pruneLogDisplays();
    bool startTimer = false;
    for (const auto &display : Log::displays) {
        // Update the display, but only if it accepts this message type.
        if (display.acceptedTypes.contains(type)) {
            display.icon->setPixmap(icons.value(type));
            display.statusBar->clearMessage();
            display.message->setText(message);
            startTimer = true;
        }
    }

    // Auto-hide status bar messages after a set period of time
    if (startTimer) Log::displayClearTimer.start(5000);
}

void clearLogDisplays() {
    pruneLogDisplays();
    for (const auto &display : Log::displays) {
        display.icon->setPixmap(QPixmap());
        display.message->setText(QString());
    }
}

template <typename T>
void logToConsole(const QString &message, T type) {
    stream(type) << colorizeMessage(message, type).toStdString() << std::endl;
}

template <typename T>
QString toPrefixedMessage(const QString &message, T type) {
    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    return QString("%1 %2 %3").arg(now).arg(prefix(type)).arg(message);
}

template <typename T>
void log(const QString &message, T type) {
    const QString fullMessage = toPrefixedMessage(message, type);
    logToConsole(fullMessage, type);

    if (!Log::initialized) return;

    updateLogDisplays(message, type);

    Log::textStream << fullMessage << Qt::endl;
}

void logQt(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    log(qFormatLogMessage(type, context, msg), type);
}

void logInfo(const QString &message) {
    log(message, LogType::LOG_INFO);
}

void logWarn(const QString &message) {
    log(message, LogType::LOG_WARN);
}

void logError(const QString &message) {
    Log::mostRecentError = message;
    log(message, LogType::LOG_ERROR);
}

QString getLogPath() {
    return Log::path;
}

QString getMostRecentError() {
    return Log::mostRecentError;
}

bool cleanupLargeLog() {
    return Log::file.size() >= 20000000 && Log::file.resize(0);
}

void logInit() {
    if (Log::initialized) return;

    QString settingsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(settingsPath);
    if (!dir.exists())
        dir.mkpath(settingsPath);
    Log::path = dir.absoluteFilePath(QStringLiteral("porymap.log"));
    Log::file.setFileName(Log::path);
    if (!Log::file.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    Log::textStream.setDevice(&Log::file);

    QObject::connect(&Log::displayClearTimer, &QTimer::timeout, [=] {
        clearLogDisplays();
    });

    qInstallMessageHandler(logQt);

    Log::initialized = true;

    if (cleanupLargeLog()) {
        logWarn(QString("Previous log file %1 was cleared due to being over 20MB in size.").arg(Log::path));
    }
}
