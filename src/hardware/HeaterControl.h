#ifndef HEATER_CONTROL_H
#define HEATER_CONTROL_H

#include "../config/SystemConfig.h"
#include "../sensors/SafetySystem.h"
#include <PID_v1.h>

// La classe HeaterControl gère le contrôle du chauffage (tapis chauffant).
// Elle peut fonctionner en mode ON/OFF (hystérésis) ou en mode PID/PWM.
// C'est une classe statique pour un accès global.
class HeaterControl {
public:
    /**
     * @brief Initialise le contrôle du chauffage.
     * @param heaterPin La broche de commande du chauffage.
     * @return true si l'initialisation a réussi, false sinon.
     */
    static bool initialize(int heaterPin);

    /**
     * @brief Met à jour la logique de contrôle du chauffage.
     * @param currentTemp La température actuelle (en int16_t, ex: 255 pour 25.5°C).
     * @param targetTemp La température de consigne (en int16_t).
     * @param config La configuration système actuelle.
     * @param safety L'état du système de sécurité.
     */
    static void updateControl(int16_t currentTemp, int16_t targetTemp, const SystemConfig& config, const SafetySystem& safety);

    /**
     * @brief Arrête immédiatement le chauffage en cas d'urgence.
     */
    static void emergencyStop();

    /**
     * @brief Règle les paramètres du contrôleur PID.
     * @param kp Le gain proportionnel.
     * @param ki Le gain intégral.
     * @param kd Le gain dérivé.
     */
    static void setPIDParameters(float kp, float ki, float kd);
    
    // --- État et statistiques ---

    /**
     * @brief Obtient la puissance de sortie actuelle du chauffage (0-255).
     * @return La puissance de sortie.
     */
    static float getCurrentOutput() { return currentOutput; }

    /**
     * @brief Vérifie si le chauffage est actuellement en marche.
     * @return true si le chauffage est actif, false sinon.
     */
    static bool isHeating() { return currentOutput > 0; }

    /**
     * @brief Obtient le mode de fonctionnement actuel.
     * @return "PID" ou "ON/OFF".
     */
    static String getMode() { return pidMode ? "PID" : "ON/OFF"; }

    /**
     * @brief Obtient le temps du dernier basculement en mode ON/OFF.
     * @return Le temps en millisecondes.
     */
    static unsigned long getLastToggleTime() { return lastToggleTime; }

    /**
     * @brief Obtient le temps total de fonctionnement du chauffage.
     * @return Le temps total en millisecondes.
     */
    static unsigned long getTotalOnTime() { return totalOnTime; }

    /**
     * @brief Réinitialise les statistiques de fonctionnement.
     */
    static void resetStatistics();
    
    // --- Configuration ---

    /**
     * @brief Règle la valeur de l'hystérésis pour le mode ON/OFF.
     * @param hysteresis La valeur de l'hystérésis en int16_t (ex: 3 pour 0.3°C).
     */
    static void setHysteresis(int16_t hysteresis) { hysteresisValue = hysteresis; }

    /**
     * @brief Active ou désactive le mode PID/PWM.
     * @param enabled true pour activer le mode PID, false pour le mode ON/OFF.
     */
    static void setPWMMode(bool enabled);

    /**
     * @brief Effectue un test du chauffage pendant une durée donnée.
     * @param duration_ms La durée du test en millisecondes.
     */
    static void testHeater(int duration_ms); // Pour tests/diagnostic
    
private:
    static PID* pidController;
    static int pin;
    static double pidInput, pidOutput; // pidInput sera converti de int16_t à double
    static int16_t pidSetpoint; // Changé en int16_t
    static float currentOutput;
    static int16_t hysteresisValue; // Changé en int16_t
    static bool pidMode;
    static bool manualCycleOn;
    static unsigned long lastToggleTime;
    static unsigned long totalOnTime;
    static unsigned long lastOnTime;
    
    static void applyOutput(float output);
    static void updateOnOffControl(int16_t currentTemp, int16_t targetTemp); // Accepte int16_t
    static void updatePIDControl(int16_t currentTemp, int16_t targetTemp); // Accepte int16_t
    static bool isSafeToHeat(const SafetySystem& safety);
};

#endif