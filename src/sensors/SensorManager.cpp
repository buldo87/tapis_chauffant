#include "SensorManager.h"
#include "SafetySystem.h"
#include "../utils/Logger.h"

// Variables statiques
Adafruit_SHT31 SensorManager::sht31 = Adafruit_SHT31();
SemaphoreHandle_t SensorManager::i2cMutex = NULL;
int16_t SensorManager::currentTemp = 0; // Changé en int16_t
float SensorManager::currentHum = NAN;
int16_t SensorManager::maxTemp = -32768; // Changé en int16_t (min value)
int16_t SensorManager::minTemp = 32767;  // Changé en int16_t (max value)
float SensorManager::maxHum = -INFINITY;
float SensorManager::minHum = INFINITY;
bool SensorManager::dataValid = false;
unsigned long SensorManager::lastUpdateTime = 0;
int SensorManager::consecutiveFailures = 0;

bool SensorManager::initialize() {
    if (!sht31.begin(0x44)) {
        LOG_ERROR("SENSORS", "Capteur SHT31 non trouvé !");
        return false;
    }
    
    resetStatistics();
    LOG_INFO("SENSORS", "SensorManager initialisé");
    return true;
}

bool SensorManager::updateSensors() {
    float tempFloat, humFloat;
    
    if (readSensorWithRetry(tempFloat, humFloat)) {
        // Convertir float en int16_t dès la lecture
        int16_t tempInt = (int16_t)(tempFloat * 10.0f);

        if (validateReading(tempInt, humFloat)) {
            currentTemp = tempInt;
            currentHum = humFloat;
            dataValid = true;
            lastUpdateTime = millis();
            consecutiveFailures = 0;
            
            updateStatistics(currentTemp, currentHum);
            
            LOG_DEBUG("SENSORS", "Capteurs mis à jour: %.1f°C, %.0f%%", (float)currentTemp / 10.0f, currentHum);
            return true;
        } else {
            LOG_WARN("SENSORS", "Lecture capteur invalide: %.1f°C, %.0f%%", tempFloat, humFloat);
        }
    }
    
    consecutiveFailures++;
    if (consecutiveFailures >= SafetyConstants::MAX_CONSECUTIVE_FAILURES) {
        dataValid = false;
        LOG_ERROR("SENSORS", "Capteurs en échec après %d tentatives", consecutiveFailures);
    }
    
    return false;
}

bool SensorManager::readTemperatureHumidity(float& temperature, float& humidity) {
    return readSensorWithRetry(temperature, humidity);
}

bool SensorManager::readSensorWithRetry(float& temp, float& hum, int maxRetries) {
    if (!i2cMutex) {
        LOG_ERROR("SENSORS", "Mutex I2C non initialisé");
        return false;
    }
    
    // Tentative d'acquisition du mutex avec timeout
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOG_WARN("SENSORS", "Timeout acquisition mutex I2C pour capteur");
        return false;
    }
    
    bool success = false;
    for (int attempt = 0; attempt < maxRetries && !success; attempt++) {
        if (attempt > 0) {
            LOG_DEBUG("SENSORS", "Tentative %d/%d de lecture capteur", attempt + 1, maxRetries);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        temp = sht31.readTemperature();
        hum = sht31.readHumidity();
        
        if (!isnan(temp) && !isnan(hum)) {
            success = true;
            LOG_DEBUG("SENSORS", "Lecture capteur réussie : %.1f°C, %.0f%%", temp, hum);
        } else {
            LOG_WARN("SENSORS", "Échec lecture capteur (tentative %d)", attempt + 1);
        }
    }
    // Libération du mutex
    xSemaphoreGive(i2cMutex);
    return success;

}

bool SensorManager::validateReading(int16_t temp, float hum) { // temp est maintenant int16_t
    // Validation des plages de température et humidité
    if (temp < (int16_t)(-40.0f * 10) || temp > (int16_t)(100.0f * 10)) { // Comparaison avec int16_t
        LOG_WARN("SENSORS", "Température hors plage: %.1f°C", (float)temp / 10.0f);
        return false;
    }
    if (hum < 0.0f || hum > 100.0f) {
        LOG_WARN("SENSORS", "Humidité hors plage: %.0f%%", hum);
        return false;
    }

    // Validation des variations brutales
    if (dataValid) {
        int16_t tempDiff = abs(temp - currentTemp); // Calcul avec int16_t
        float humDiff = abs(hum - currentHum);
        
        if (tempDiff > (int16_t)(10.0f * 10)) { // Comparaison avec int16_t
            LOG_WARN("SENSORS", "Variation température suspecte: %.1f°C -> %.1f°C", (float)currentTemp / 10.0f, (float)temp / 10.0f);
            return false;
        }
        
        if (humDiff > 20.0f) {
            LOG_WARN("SENSORS", "Variation humidité suspecte: %.0f%% -> %.0f%%", currentHum, hum);
            return false;
        }
    }

    return true;

}

void SensorManager::updateStatistics(int16_t temp, float hum) { // temp est maintenant int16_t
    // Mise à jour des statistiques
    if (temp > maxTemp) maxTemp = temp;
    if (temp < minTemp) minTemp = temp;
    if (hum > maxHum) maxHum = hum;
    if (hum < minHum) minHum = hum;
}

void SensorManager::resetStatistics() {
    maxTemp = -32768; // int16_t min value
    minTemp = 32767;  // int16_t max value
    maxHum = -INFINITY;
    minHum = INFINITY;
    consecutiveFailures = 0;
    dataValid = false;
    LOG_INFO("SENSORS", "Statistiques capteurs réinitialisées");
}
