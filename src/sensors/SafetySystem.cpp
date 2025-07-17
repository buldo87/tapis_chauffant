#include "SafetySystem.h"
#include "../utils/Logger.h"
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display; // Référence à l'écran défini dans main

// Définition des membres statiques de SafetySystem
SafetyLevel SafetySystem::currentLevel = SAFETY_NORMAL;
unsigned long SafetySystem::lastSensorRead = 0;
unsigned long SafetySystem::lastValidTemperature = 0;
unsigned long SafetySystem::lastValidHumidity = 0;
unsigned long SafetySystem::safetyActivatedTime = 0;
int SafetySystem::consecutiveFailures = 0;
int SafetySystem::temperatureOutOfRangeCount = 0;
int SafetySystem::humidityOutOfRangeCount = 0;
bool SafetySystem::emergencyShutdown = false;
String SafetySystem::lastErrorMessage = "";
int16_t SafetySystem::lastKnownGoodTemp = 220; // 22.0°C
float SafetySystem::lastKnownGoodHum = 50.0f;

void SafetySystem::initialize() {
    currentLevel = SAFETY_NORMAL;
    lastSensorRead = millis();
    lastValidTemperature = millis();
    lastValidHumidity = millis();
    consecutiveFailures = 0;
    temperatureOutOfRangeCount = 0;
    humidityOutOfRangeCount = 0;
    emergencyShutdown = false;
    lastErrorMessage = "";
    lastKnownGoodTemp = 220; // 22.0°C
    lastKnownGoodHum = 50.0f;
    
    LOG_INFO("SAFETY", "Système de sécurité initialisé");
}

void SafetySystem::checkConditions(int16_t currentTemp, float currentHum) {
    unsigned long now = millis();
    
    // Vérifier le timeout des capteurs
    if (now - lastSensorRead > SafetyConstants::SENSOR_TIMEOUT) {
        escalateSafety(SAFETY_CRITICAL, 
                      "Capteurs non réactifs depuis " + String((now - lastSensorRead)/1000) + "s");
        return;
    }
    
    // Vérifier la température
    if (!isnan((float)currentTemp / 10.0f)) { // Convert to float for isnan check
        lastValidTemperature = now;
        lastKnownGoodTemp = currentTemp;
        
        if (currentTemp >= (int16_t)(SafetyConstants::TEMP_EMERGENCY_HIGH * 10) || 
            currentTemp <= (int16_t)(SafetyConstants::TEMP_EMERGENCY_LOW * 10)) {
            escalateSafety(SAFETY_EMERGENCY, 
                          "Température critique: " + String((float)currentTemp / 10.0f, 1) + "°C");
            return;
        } else if (currentTemp >= (int16_t)(SafetyConstants::TEMP_WARNING_HIGH * 10) || 
                   currentTemp <= (int16_t)(SafetyConstants::TEMP_WARNING_LOW * 10)) {
            escalateSafety(SAFETY_WARNING, 
                          "Température d'alerte: " + String((float)currentTemp / 10.0f, 1) + "°C");
        } else {
            temperatureOutOfRangeCount = 0;
        }
    } else {
        consecutiveFailures++;
        if (consecutiveFailures >= SafetyConstants::MAX_CONSECUTIVE_FAILURES) {
            escalateSafety(SAFETY_CRITICAL, 
                          "Échecs consécutifs de lecture température: " + String(consecutiveFailures));
            return;
        }
    }
    
    // Vérifier l'humidité
    if (!isnan(currentHum)) {
        lastValidHumidity = now;
        lastKnownGoodHum = currentHum;
        
        if (currentHum >= SafetyConstants::HUM_EMERGENCY_HIGH || 
            currentHum <= SafetyConstants::HUM_EMERGENCY_LOW) {
            escalateSafety(SAFETY_WARNING, 
                          "Humidité critique: " + String(currentHum, 0) + "%");
        }
    }
    
    // Vérifier les variations suspectes
    if (abs(currentTemp - lastKnownGoodTemp) > (int16_t)(10.0f * 10)) { // Compare int16_t
        temperatureOutOfRangeCount++;
        if (temperatureOutOfRangeCount >= 3) {
            escalateSafety(SAFETY_WARNING, 
                          "Variation température suspecte: " + String((float)currentTemp / 10.0f, 1) + "°C vs " + 
                          String((float)lastKnownGoodTemp / 10.0f, 1) + "°C");
        }
    }
    
    // Tentative de downgrade si les conditions se sont améliorées
    if (currentLevel > SAFETY_NORMAL && 
        now - safetyActivatedTime > SafetyConstants::SAFETY_RESET_DELAY) {
        if (currentTemp > (int16_t)(SafetyConstants::TEMP_WARNING_LOW * 10) && 
            currentTemp < (int16_t)(SafetyConstants::TEMP_WARNING_HIGH * 10) && 
            currentHum > SafetyConstants::HUM_EMERGENCY_LOW && 
            currentHum < SafetyConstants::HUM_EMERGENCY_HIGH) {
            downgradeSafety();
        }
    }
}

void SafetySystem::escalateSafety(SafetyLevel newLevel, const String& reason) {
    unsigned long now = millis();
    
    if (newLevel > currentLevel) {
        currentLevel = newLevel;
        safetyActivatedTime = now;
        lastErrorMessage = reason;
        
        LOG_ERROR("SAFETY", "SÉCURITÉ NIVEAU %d: %s", newLevel, reason.c_str());
        
        switch (newLevel) {
            case SAFETY_WARNING:
                activateWarningMode(reason);
                break;
            case SAFETY_CRITICAL:
                activateCriticalMode(reason);
                break;
            case SAFETY_EMERGENCY:
                activateEmergencyMode(reason);
                break;
            default:
                break;
        }
    }
}

void SafetySystem::activateWarningMode(const String& reason) {
    LOG_WARN("SAFETY", "MODE ALERTE ACTIVÉ: %s", reason.c_str());
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ALERTE! ");
    display.setCursor(0, 12);
    display.println(reason.substring(0, 21));
    display.setCursor(0, 24);
    display.printf("Temp: %.1fC", (float)lastKnownGoodTemp / 10.0f);
    display.setCursor(0, 36);
    display.printf("Hum: %.0f%%", lastKnownGoodHum);
    display.setCursor(0, 48);
    display.println("Surveillance++");
    display.display();
}

void SafetySystem::activateCriticalMode(const String& reason) {
    LOG_ERROR("SAFETY", "MODE CRITIQUE ACTIVÉ: %s", reason.c_str());
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("MODE CRITIQUE");
    display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
    display.setCursor(0, 15);
    display.println("Chauffage OFF");
    display.setCursor(0, 27);
    display.println(reason.substring(0, 21));
    display.setCursor(0, 39);
    display.println("Verification...");
    display.setCursor(0, 51);
    display.printf("T:%.1f H:%.0f%%", (float)lastKnownGoodTemp / 10.0f, lastKnownGoodHum);
    display.display();
}

void SafetySystem::activateEmergencyMode(const String& reason) {
    LOG_ERROR("SAFETY", "MODE URGENCE ACTIVÉ: %s", reason.c_str());
    
    emergencyShutdown = true;
    
    static bool blinkState = false;
    blinkState = !blinkState;
    
    display.clearDisplay();
    if (blinkState) {
        display.fillScreen(SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    } else {
        display.setTextColor(SSD1306_WHITE);
    }
    
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("URGENCE! ");
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("ARRET COMPLET");
    display.setCursor(0, 32);
    display.println(reason.substring(0, 21));
    display.setCursor(0, 44);
    display.println("Verif. capteurs");
    display.display();
}

void SafetySystem::downgradeSafety() {
    SafetyLevel oldLevel = currentLevel;
    
    if (currentLevel > SAFETY_NORMAL) {
        currentLevel = (SafetyLevel)(currentLevel - 1);
        LOG_INFO("SAFETY", "Niveau de sécurité réduit de %d à %d", oldLevel, currentLevel);
        
        if (currentLevel == SAFETY_NORMAL) {
            exitSafeMode();
        }
    }
}

void SafetySystem::exitSafeMode() {
    LOG_INFO("SAFETY", "RETOUR AU MODE NORMAL");
    
    emergencyShutdown = false;
    consecutiveFailures = 0;
    temperatureOutOfRangeCount = 0;
    humidityOutOfRangeCount = 0;
    lastErrorMessage = "";
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SYSTEME OK");
    display.setCursor(0, 15);
    display.println("Reprise normale");
    display.setCursor(0, 30);
    display.printf("Temp: %.1fC", (float)lastKnownGoodTemp / 10.0f);
    display.setCursor(0, 45);
    display.printf("Hum: %.0f%%", lastKnownGoodHum);
    display.display();
    delay(2000);
}

void SafetySystem::resetSafety() {
    initialize();
    LOG_INFO("SAFETY", "Système de sécurité réinitialisé");
}