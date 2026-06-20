#include "logwriter.h"

#include <QDateTime>

#include <iostream>

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

std::ostream& consoleStream(LogType type) {
    switch (type) {
    case LogType::LOG_INFO:
        return std::cout;
    case LogType::LOG_WARN:
    case LogType::LOG_ERROR:
        return std::cerr;
    }
}

std::ostream& consoleStream(QtMsgType type) {
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




LogWriter::LogWriter(QObject *parent)
    : QObject(parent) {};

LogWriter::LogWriter(const QString& path, QObject *parent)
    : LogWriter(parent)
{
    setPath(path);
};

bool LogWriter::setPath(const QString& path) {
    QMutexLocker locker(&m_mutex);

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) return false;

    m_textStream.setDevice(&m_file);

    if (m_file.size() >= 20000000 && m_file.resize(0)) {
        // TODO logWarn(QString("Previous log file %1 was cleared due to being over 20MB in size.").arg(Log::path));
    }

    return true;
}

QString LogWriter::path() const {
    QMutexLocker locker(&m_mutex);
    return m_file.fileName();
}

void LogWriter::setConsoleEnabled(bool consoleEnabled) {
    QMutexLocker locker(&m_mutex);
    m_consoleEnabled = consoleEnabled;
}

bool LogWriter::consoleEnabled() const {
    QMutexLocker locker(&m_mutex);
    return m_consoleEnabled;
}

template <typename T>
void LogWriter::writeInternal(const QString& message, T type) {
    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString fullMessage = QString("%1 %2 %3").arg(now).arg(prefix(type)).arg(message);

    if (m_consoleEnabled) {
        consoleStream(type) << colorizeMessage(fullMessage, type).toStdString() << std::endl;
    }
    if (m_textStream.device()) {
        m_textStream << fullMessage << Qt::endl;
    }
}

void LogWriter::write(const QString& message, LogType type) {
    QMutexLocker locker(&m_mutex);
    writeInternal(message, type);
}

void LogWriter::writeQt(const QString& message, QtMsgType type) {
    QMutexLocker locker(&m_mutex);
    writeInternal(message, type);
}
