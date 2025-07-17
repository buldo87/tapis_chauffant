#ifndef SAFETY_SYSTEM_H
#define SAFETY_SYSTEM_H

#include "../config/SystemConfig.h"

// La classe SafetySystem surveille en permanence les conditions du système
// pour prévenir les situations dangereuses (surchauffe, panne de capteur, etc.).
// Elle est conçue comme une classe statique pour une gestion globale de la sécurité.
class SafetySystem {
public:
    // Membres de données statiques
    static SafetyLevel currentLevel;
    static unsigned long lastSensorRead;
    static unsigned long lastValidTemperature;
    static unsigned long lastValidHumidity;
    static unsigned long safetyActivatedTime;
    static int consecutiveFailures;
    static int temperatureOutOfRangeCount;
    static int humidityOutOfRangeCount;
    static bool emergencyShutdown;
    static String lastErrorMessage;
    static int16_t lastKnownGoodTemp; // Changé en int16_t
    static float lastKnownGoodHum;

    /**
     * @brief Initialise l'état du système de sécurité.
     */
    static void initialize();

    /**
     * @brief Vérifie les conditions actuelles et met à jour le niveau de sécurité.
     * @param currentTemp Température actuelle (en int16_t).
     * @param currentHum Humidité actuelle.
     */
    static void checkConditions(int16_t currentTemp, float currentHum);

    /**
     * @brief Fait monter le niveau de sécurité.
     * @param newLevel Le nouveau niveau de sécurité.
     * @param reason La raison de l'escalade.
     */
    static void escalateSafety(SafetyLevel newLevel, const String& reason);

    /**
     * @brief Fait descendre le niveau de sécurité.
     */
    static void downgradeSafety();

    /**
     * @brief Réinitialise complètement le système de sécurité.
     */
    static void resetSafety();

    /**
     * @brief Vérifie si le système est en arrêt d'urgence.
     * @return true si le système est en arrêt d'urgence, false sinon.
     */
    static bool isEmergencyShutdown();
    static int getConsecutiveFailures();
    static unsigned long getLastSensorReadTime();
    static int16_t getLastKnownGoodTemp();
    static float getLastKnownGoodHum();
    static String getLastErrorMessage();

    /**
     * @brief Obtient le niveau de sécurité actuel.
     * @return Le niveau de sécurité actuel.
     */
    static SafetyLevel getCurrentLevel() { return currentLevel; }
    
private:
    static void activateWarningMode(const String& reason);
    static void activateCriticalMode(const String& reason);
    static void activateEmergencyMode(const String& reason);
    static void exitSafeMode();
};

#endif