#ifndef SAFETY_SYSTEM_H
#define SAFETY_SYSTEM_H

#include "../config/SystemConfig.h"

// La classe SafetySystem surveille en permanence les conditions du système
// pour prévenir les situations dangereuses (surchauffe, panne de capteur, etc.).
// Elle est conçue comme une classe statique pour une gestion globale de la sécurité.
class SafetySystem {
public:
    /**
     * @brief Initialise l'état du système de sécurité.
     * @param safety Référence à la structure de l'état de sécurité.
     */
    static void initialize(SafetySystem& safety);

    /**
     * @brief Vérifie les conditions actuelles et met à jour le niveau de sécurité.
     * @param safety Référence à la structure de l'état de sécurité.
     * @param currentTemp Température actuelle.
     * @param currentHum Humidité actuelle.
     */
    static void checkConditions(SafetySystem& safety, float currentTemp, float currentHum);

    /**
     * @brief Fait monter le niveau de sécurité.
     * @param safety Référence à la structure de l'état de sécurité.
     * @param newLevel Le nouveau niveau de sécurité.
     * @param reason La raison de l'escalade.
     */
    static void escalateSafety(SafetySystem& safety, SafetyLevel newLevel, const String& reason);

    /**
     * @brief Fait descendre le niveau de sécurité.
     * @param safety Référence à la structure de l'état de sécurité.
     */
    static void downgradeSafety(SafetySystem& safety);

    /**
     * @brief Réinitialise complètement le système de sécurité.
     * @param safety Référence à la structure de l'état de sécurité.
     */
    static void resetSafety(SafetySystem& safety);

    /**
     * @brief Vérifie si le système est en arrêt d'urgence.
     * @param safety Référence à la structure de l'état de sécurité.
     * @return true si le système est en arrêt d'urgence, false sinon.
     */
    static bool isEmergencyShutdown(const SafetySystem& safety) { return safety.emergencyShutdown; }

    /**
     * @brief Obtient le niveau de sécurité actuel.
     * @param safety Référence à la structure de l'état de sécurité.
     * @return Le niveau de sécurité actuel.
     */
    static SafetyLevel getCurrentLevel(const SafetySystem& safety) { return safety.currentLevel; }
    
private:
    static void activateWarningMode(SafetySystem& safety, const String& reason);
    static void activateCriticalMode(SafetySystem& safety, const String& reason);
    static void activateEmergencyMode(SafetySystem& safety, const String& reason);
    static void exitSafeMode(SafetySystem& safety);
};

#endif