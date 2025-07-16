#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "SystemConfig.h"

// Énumération des niveaux de log
enum LogLevel {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4
};

// Variable globale pour le niveau de log actuel
extern LogLevel g_currentLogLevel;

// Fonction pour définir le niveau de log
void setLogLevel(LogLevel level);

// Fonction pour convertir le niveau de log en chaîne de caractères
const char* logLevelToString(LogLevel level);

// La macro de logging principale
#define LOG(level, module, format, ...) \
    if (level <= g_currentLogLevel) { \
        Serial.printf("[%s] [%-12s] " format "\n", logLevelToString(level), module, ##__VA_ARGS__); \
    }

// Macros spécifiques pour chaque niveau
#define LOG_ERROR(module, format, ...)   LOG(LOG_LEVEL_ERROR, module, format, ##__VA_ARGS__)
#define LOG_WARN(module, format, ...)    LOG(LOG_LEVEL_WARN, module, format, ##__VA_ARGS__)
#define LOG_INFO(module, format, ...)    LOG(LOG_LEVEL_INFO, module, format, ##__VA_ARGS__)
#define LOG_DEBUG(module, format, ...)   LOG(LOG_LEVEL_DEBUG, module, format, ##__VA_ARGS__)

#endif // LOGGER_H
