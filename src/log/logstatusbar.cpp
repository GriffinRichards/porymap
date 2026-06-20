#include "logstatusbar.h"
#include "logsignaler.h"

LogStatusBar::LogStatusBar(QWidget* parent) : QStatusBar(parent) {
    addWidget(&m_icon);
    addWidget(&m_message);
    connect(&m_timer, &QTimer::timeout, this, &LogStatusBar::clearLogMessage);
    connect(LogSignaler::instance(), &LogSignaler::logged, this, &LogStatusBar::showLogMessage);
};

LogStatusBar::~LogStatusBar() {
    m_timer.stop();
}

void LogStatusBar::showLogMessage(const QString& message, LogType type) {
    // Filter out irrelevant log types
    if (!m_acceptedLogTypes.contains(type)) return;

    static const QMap<LogType, QPixmap> icons = {
        {LogType::LOG_INFO,  QPixmap(QStringLiteral(":/icons/information.ico"))},
        {LogType::LOG_WARN,  QPixmap(QStringLiteral(":/icons/warning.ico"))},
        {LogType::LOG_ERROR, QPixmap(QStringLiteral(":/icons/error.ico"))},
    };

    // Display message
    m_icon.setPixmap(icons.value(type));
    clearMessage();
    m_message.setText(message);

    // Auto-hide message after a set period of time
    m_timer.start(m_timeout);
}

void LogStatusBar::clearLogMessage() {
    m_icon.setPixmap(QPixmap());
    m_message.setText(QString());
}
