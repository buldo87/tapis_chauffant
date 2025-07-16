#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "../config/SystemConfig.h"
#include <Adafruit_SHT31.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// La classe SensorManager gère la lecture des capteurs (température et humidité).
// Elle est conçue comme une classe statique pour un accès centralisé.
class SensorManager {
public:
    static bool initialize();
    static bool readTemperatureHumidity(float& temperature, float& humidity);
    static bool updateSensors();
    static int16_t getCurrentTemperature() { return currentTemp; } // Retourne int16_t
    static float getCurrentHumidity() { return currentHum; }
    static bool isDataValid() { return dataValid; }
    static unsigned long getLastUpdateTime() { return lastUpdateTime; }
    
    // Statistiques
    static int16_t getMaxTemperature() { return maxTemp; } // Retourne int16_t
    static int16_t getMinTemperature() { return minTemp; } // Retourne int16_t
    static float getMaxHumidity() { return maxHum; }
    static float getMinHumidity() { return minHum; }
    static void resetStatistics();
    
    // Configuration du mutex I2C
    static void setI2CMutex(SemaphoreHandle_t mutex) { i2cMutex = mutex; }

private:
    static Adafruit_SHT31 sht31;
    static SemaphoreHandle_t i2cMutex;
    static int16_t currentTemp, maxTemp, minTemp; // Changé en int16_t
    static float currentHum, maxHum, minHum;
    static bool dataValid;
    static unsigned long lastUpdateTime;
    static int consecutiveFailures;
    
    static bool readSensorWithRetry(float& temp, float& hum, int maxRetries = 3);
    static bool validateReading(float temp, float hum);
    static void updateStatistics(int16_t temp, float hum); // Accepte int16_t pour temp
};

#endif