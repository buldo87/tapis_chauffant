#include "SafetySystem.h"
#include "../utils/Logger.h"
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display; // Référence à l'écran défini dans main

void SafetySystem::initialize(SafetySystem& safety) {
    safety.currentLevel = SAFETY_NORMAL;
    safety.lastSensorRead = millis();
    safety.lastValidTemperature = millis();
    safety.lastValidHumidity = millis();
    safety.consecutiveFailures = 0;
    safety.temperatureOutOfRangeCount = 0;
    safety.humidityOutOfRangeCount = 0;
    safety.emergencyShutdown = false;
    safety.lastErrorMessage = "";
    safety.lastKnownGoodTemp = 22.0f;
    safety.lastKnownGoodHum = 50.0f;
    
    LOG_INFO("SAFETY", "Système de sécurité initialisé");
}

void SafetySystem::checkConditions(SafetySystem& safety, float currentTemp, float currentHum) {
    unsigned long now = millis();
    
    // Vérifier le timeout des capteurs
    if (now - safety.lastSensorRead > SafetyConstants::SENSOR_TIMEOUT) {
        escalateSafety(safety, SAFETY_CRITICAL, 
                      "Capteurs non réactifs depuis " + String((now - safety.lastSensorRead)/1000) + "s");
        return;
    }
    
    // Vérifier la température
    if (!isnan(currentTemp)) {
        safety.lastValidTemperature = now;
        safety.lastKnownGoodTemp = currentTemp;
        
        if (currentTemp >= SafetyConstants::TEMP_EMERGENCY_HIGH || 
            currentTemp <= SafetyConstants::TEMP_EMERGENCY_LOW) {
            escalateSafety(safety, SAFETY_EMERGENCY, 
                          "Température critique: " + String(currentTemp, 1) + "°C");
            return;
        } else if (currentTemp >= SafetyConstants::TEMP_WARNING_HIGH || 
                   currentTemp <= SafetyConstants::TEMP_WARNING_LOW) {
            escalateSafety(safety, SAFETY_WARNING, 
                          "Température d'alerte: " + String(currentTemp, 1) + "°C");
        } else {
            safety.temperatureOutOfRangeCount = 0;
        }
    } else {
        safety.consecutiveFailures++;
        if (safety.consecutiveFailures >= SafetyConstants::MAX_CONSECUTIVE_FAILURES) {
            escalateSafety(safety, SAFETY_CRITICAL, 
                          "Échecs consécutifs de lecture température: " + String(safety.consecutiveFailures));
            return;
        }
    }
    
    // Vérifier l'humidité
    if (!isnan(currentHum)) {
        safety.lastValidHumidity = now;
        safety.lastKnownGoodHum = currentHum;
        
        if (currentHum >= SafetyConstants::HUM_EMERGENCY_HIGH || 
            currentHum <= SafetyConstants::HUM_EMERGENCY_LOW) {
            escalateSafety(safety, SAFETY_WARNING, 
                          "Humidité critique: " + String(currentHum, 0) + "%");
        }
    }
    
    // Vérifier les variations suspectes
    if (abs(currentTemp - safety.lastKnownGoodTemp) > 10.0f) {
        safety.temperatureOutOfRangeCount++;
        if (safety.temperatureOutOfRangeCount >= 3) {
            escalateSafety(safety, SAFETY_WARNING, 
                          "Variation température suspecte: " + String(currentTemp, 1) + "°C vs " + 
                          String(safety.lastKnownGoodTemp, 1) + "°C");
        }
    }
    
    // Tentative de downgrade si les conditions se sont améliorées
    if (safety.currentLevel > SAFETY_NORMAL && 
        now - safety.safetyActivatedTime > SafetyConstants::SAFETY_RESET_DELAY) {
        if (currentTemp > SafetyConstants::TEMP_WARNING_LOW && 
            currentTemp < SafetyConstants::TEMP_WARNING_HIGH && 
            currentHum > SafetyConstants::HUM_EMERGENCY_LOW && 
            currentHum < SafetyConstants::HUM_EMERGENCY_HIGH) {
            downgradeSafety(safety);
        }
    }
}

void SafetySystem::escalateSafety(SafetySystem& safety, SafetyLevel newLevel, const String& reason) {
    unsigned long now = millis();
    
    if (newLevel > safety.currentLevel) {
        safety.currentLevel = newLevel;
        safety.safetyActivatedTime = now;
        safety.lastErrorMessage = reason;
        
        LOG_ERROR("SAFETY", "SÉCURITÉ NIVEAU %d: %s", newLevel, reason.c_str());
        
        switch (newLevel) {
            case SAFETY_WARNING:
                activateWarningMode(safety, reason);
                break;
            case SAFETY_CRITICAL:
                activateCriticalMode(safety, reason);
                break;
            case SAFETY_EMERGENCY:
                activateEmergencyMode(safety, reason);
                break;
            default:
                break;
        }
    }
}

void SafetySystem::activateWarningMode(SafetySystem& safety, const String& reason) {
    LOG_WARN("SAFETY", "MODE ALERTE ACTIVÉ: %s", reason.c_str());
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ALERTE! ");
    display.setCursor(0, 12);
    display.println(reason.substring(0, 21));
    display.setCursor(0, 24);
    display.printf("Temp: %.1fC", safety.lastKnownGoodTemp);
    display.setCursor(0, 36);
    display.printf("Hum: %.0f%%", safety.lastKnownGoodHum);
    display.setCursor(0, 48);
    display.println("Surveillance++");
    display.display();
}

void SafetySystem::activateCriticalMode(SafetySystem& safety, const String& reason) {
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
    display.printf("T:%.1f H:%.0f%%", safety.lastKnownGoodTemp, safety.lastKnownGoodHum);
    display.display();
}

void SafetySystem::activateEmergencyMode(SafetySystem& safety, const String& reason) {
    LOG_ERROR("SAFETY", "MODE URGENCE ACTIVÉ: %s", reason.c_str());
    
    safety.emergencyShutdown = true;
    
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

void SafetySystem::downgradeSafety(SafetySystem& safety) {
    SafetyLevel oldLevel = safety.currentLevel;
    
    if (safety.currentLevel > SAFETY_NORMAL) {
        safety.currentLevel = (SafetyLevel)(safety.currentLevel - 1);
        LOG_INFO("SAFETY", "Niveau de sécurité réduit de %d à %d", oldLevel, safety.currentLevel);
        
        if (safety.currentLevel == SAFETY_NORMAL) {
            exitSafeMode(safety);
        }
    }
}

void SafetySystem::exitSafeMode(SafetySystem& safety) {
    LOG_INFO("SAFETY", "RETOUR AU MODE NORMAL");
    
    safety.emergencyShutdown = false;
    safety.consecutiveFailures = 0;
    safety.temperatureOutOfRangeCount = 0;
    safety.humidityOutOfRangeCount = 0;
    safety.lastErrorMessage = "";
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SYSTEME OK");
    display.setCursor(0, 15);
    display.println("Reprise normale");
    display.setCursor(0, 30);
    display.printf("Temp: %.1fC", safety.lastKnownGoodTemp);
    display.setCursor(0, 45);
    display.printf("Hum: %.0f%%", safety.lastKnownGoodHum);
    display.display();
    delay(2000);
}

void SafetySystem::resetSafety(SafetySystem& safety) {
    initialize(safety);
    LOG_INFO("SAFETY", "Système de sécurité réinitialisé");
}
