#include "HeaterControl.h"
#include "../utils/Logger.h"

// Variables statiques
PID* HeaterControl::pidController = nullptr;
int HeaterControl::pin = -1;
double HeaterControl::pidInput = 0;
double HeaterControl::pidOutput = 0;
int16_t HeaterControl::pidSetpoint = 230; // Changé en int16_t
float HeaterControl::currentOutput = 0;
int16_t HeaterControl::hysteresisValue = 3; // Changé en int16_t (0.3 * 10)
bool HeaterControl::pidMode = false;
bool HeaterControl::manualCycleOn = false;
unsigned long HeaterControl::lastToggleTime = 0;
unsigned long HeaterControl::totalOnTime = 0;
unsigned long HeaterControl::lastOnTime = 0;

bool HeaterControl::initialize(int heaterPin) {
    pin = heaterPin;
    
    if (pin < 0) {
        LOG_ERROR("HEATER", "Pin chauffage invalide");
        return false;
    }
    
    pinMode(pin, OUTPUT);
    analogWrite(pin, 0);
    
    // Initialiser le PID
    pidController = new PID(&pidInput, &pidOutput, (double*)&pidSetpoint, 2.0, 5.0, 1.0, DIRECT); // Cast pidSetpoint
    if (!pidController) {
        LOG_ERROR("HEATER", "Échec création contrôleur PID");
        return false;
    }
    
    pidController->SetMode(AUTOMATIC);
    pidController->SetOutputLimits(0, 255);
    
    resetStatistics();
    
    // Test rapide du chauffage
    LOG_INFO("HEATER", "Test rapide du chauffage...");
    analogWrite(pin, 50);
    delay(500);
    analogWrite(pin, 0);
    
    LOG_INFO("HEATER", "HeaterControl initialisé sur pin %d", pin);
    return true;
}

void HeaterControl::updateControl(int16_t currentTemp, int16_t targetTemp, const SystemConfig& config, const SafetySystem& safety) {
    if (!isSafeToHeat()) { // safety object removed
        emergencyStop();
        return;
    }
    
    // Mise à jour des paramètres
    hysteresisValue = (int16_t)(config.hysteresis * 10.0f); // Convert float to int16_t
    pidMode = config.usePWM;
    
    if (pidMode) {
        updatePIDControl(currentTemp, targetTemp);
    } else {
        updateOnOffControl(currentTemp, targetTemp);
    }
    
    // Limitation en mode warning
    float finalOutput = currentOutput;
    if (SafetySystem::getCurrentLevel() == SAFETY_WARNING) { // safety object removed
        finalOutput = min(finalOutput, 128.0f);
        LOG_WARN("HEATER", "Limitation chauffage en mode WARNING: %.0f -> %.0f", currentOutput, finalOutput);
    }
    
    applyOutput(finalOutput);
    
    // Statistiques de temps de fonctionnement
    static bool wasHeating = false;
    bool isCurrentlyHeating = (finalOutput > 0);
    
    if (isCurrentlyHeating && !wasHeating) {
        lastOnTime = millis();
    } else if (!isCurrentlyHeating && wasHeating) {
        totalOnTime += (millis() - lastOnTime);
    }
    
    wasHeating = isCurrentlyHeating;
}

void HeaterControl::updatePIDControl(int16_t currentTemp, int16_t targetTemp) {
    pidInput = (double)currentTemp / 10.0; // Convert int16_t to double for PID
    pidSetpoint = targetTemp; // pidSetpoint is already int16_t
    
    if (pidController->Compute()) {
        currentOutput = (float)pidOutput;
        LOG_DEBUG("HEATER", "PID: T=%.1f°C, Consigne=%.1f°C, Sortie=%.0f", (float)currentTemp / 10.0f, (float)targetTemp / 10.0f, currentOutput);
    }
}

void HeaterControl::updateOnOffControl(int16_t currentTemp, int16_t targetTemp) {
    int16_t maxTemp = targetTemp;
    int16_t minTemp = targetTemp - hysteresisValue;
    unsigned long now = millis();
    
    if (currentTemp >= maxTemp) {
        // Trop chaud : arrêter
        currentOutput = 0;
        manualCycleOn = false;
        LOG_DEBUG("HEATER", "ON/OFF: Arrêt chauffage - T=%.1f°C >= %.1f°C", (float)currentTemp / 10.0f, (float)maxTemp / 10.0f);
    } else if (currentTemp < minTemp) {
        // Trop froid : chauffer au maximum
        currentOutput = 255;
        manualCycleOn = false;
        LOG_DEBUG("HEATER", "ON/OFF: Chauffage max - T=%.1f°C < %.1f°C", (float)currentTemp / 10.0f, (float)minTemp / 10.0f);
    } else {
        // Zone d'hystérésis : cycles temporels
        if (manualCycleOn && now - lastToggleTime >= 990) {
            manualCycleOn = false;
            lastToggleTime = now;
            currentOutput = 0;
            LOG_DEBUG("HEATER", "ON/OFF: Fin cycle chauffage");
        } else if (!manualCycleOn && now - lastToggleTime >= 2990) {
            manualCycleOn = true;
            lastToggleTime = now;
            currentOutput = 255;
            LOG_DEBUG("HEATER", "ON/OFF: Début cycle chauffage");
        }
        // Sinon, maintenir l'état actuel
        currentOutput = manualCycleOn ? 255 : 0;
    }
}

void HeaterControl::applyOutput(float output) {
    if (pin < 0) return;
    
    currentOutput = constrain(output, 0, 255);
    analogWrite(pin, (int)currentOutput);
}

bool HeaterControl::isSafeToHeat() {
    SafetyLevel level = SafetySystem::getCurrentLevel(); // safety object removed
    
    if (SafetySystem::isEmergencyShutdown()) { // safety object removed
        return false;
    }
    
    if (level >= SAFETY_CRITICAL) {
        return false;
    }
    
    return true;
}

void HeaterControl::emergencyStop() {
    LOG_ERROR("HEATER", "ARRÊT D'URGENCE CHAUFFAGE");
    currentOutput = 0;
    applyOutput(0);
    manualCycleOn = false;
}

void HeaterControl::setPIDParameters(float kp, float ki, float kd) {
    if (pidController) {
        pidController->SetTunings(kp, ki, kd);
        LOG_INFO("HEATER", "PID mis à jour: Kp=%.2f, Ki=%.2f, Kd=%.2f", kp, ki, kd);
    }
}

void HeaterControl::setPWMMode(bool enabled) {
    pidMode = enabled;
    if (!enabled) {
        // Reset du cycle ON/OFF
        manualCycleOn = false;
        lastToggleTime = millis();
    }
    LOG_INFO("HEATER", "Mode chauffage: %s", enabled ? "PID/PWM" : "ON/OFF");
}

void HeaterControl::testHeater(int duration_ms) {
    LOG_INFO("HEATER", "Test chauffage pendant %d ms...", duration_ms);
    
    analogWrite(pin, 100); // 40% de puissance pour le test
    delay(duration_ms);
    analogWrite(pin, 0);
    
    LOG_INFO("HEATER", "Test chauffage terminé");
}

void HeaterControl::resetStatistics() {
    totalOnTime = 0;
    lastOnTime = 0;
    lastToggleTime = millis();
    LOG_INFO("HEATER", "Statistiques chauffage réinitialisées");
}
