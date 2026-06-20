#include "logsignaler.h"

LogSignaler* LogSignaler::instance() {
    static LogSignaler instance;
    return &instance;
}
