#include "Logger.h"

// Initialisation du niveau de log global (par défaut à INFO)
LogLevel g_currentLogLevel = LOG_LEVEL_INFO;

void setLogLevel(LogLevel level) {
    g_currentLogLevel = level;
}

const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default:              return "NONE";
    }
}
