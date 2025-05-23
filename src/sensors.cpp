#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>

// ==== VARIABLES PRIVÉES ====
static Adafruit_SHT31 sht31;
static float last_temperature = 0.0f;
static float last_humidity = 0.0f;
static uint32_t last_reading_time = 0;
static bool sensors_initialized = false;
static bool last_reading_valid = false;

// ==== FONCTIONS PRIVÉES ====
static bool validateSensorData(float temp, float hum);

void initSensors() {
  DEBUG_PRINTLN("Initialisation capteur SHT31...");
  
  // Initialisation du capteur SHT31
  if (sht31.begin(SHT31_ADDRESS)) {
    sensors_initialized = true;
    DEBUG_PRINTLN("SHT31 initialisé avec succès");
    
    // Test de lecture initial
    if (readSensors()) {
      DEBUG_PRINTF("Test initial - Température: %.1f°C, Humidité: %.1f%%\n", 
                   last_temperature, last_humidity);
    } else {
      DEBUG_PRINTLN("ATTENTION: Test initial de lecture échoué");
    }
  } else {
    sensors_initialized = false;
    DEBUG_PRINTLN("ERREUR: Impossible d'initialiser le SHT31");
    DEBUG_PRINTLN("Vérifiez les connexions I2C (SDA=1, SCL=2)");
  }
}

bool readSensors() {
  if (!sensors_initialized) {
    DEBUG_PRINTLN("ERREUR: Capteurs non initialisés");
    return false;
  }
  
  // Lecture du capteur SHT31
  float temp = sht31.readTemperature();
  float hum = sht31.readHumidity();
  
  // Vérification de la validité des données
  if (isnan(temp) || isnan(hum)) {
    DEBUG_PRINTLN("ERREUR: Lecture SHT31 - Valeurs NaN");
    last_reading_valid = false;
    return false;
  }
  
  // Validation des plages de valeurs
  if (!validateSensorData(temp, hum)) {
    DEBUG_PRINTF("ERREUR: Valeurs capteur hors plage - T:%.1f°C H:%.1f%%\n", temp, hum);
    last_reading_valid = false;
    return false;
  }
  
  // Stockage des valeurs valides
  last_temperature = temp;
  last_humidity = hum;
  last_reading_time = millis();
  last_reading_valid = true;
  
  DEBUG_PRINTF("Capteurs OK - T:%.1f°C H:%.1f%%\n", temp, hum);
  return true;
}

float getSensorTemperature() {
  return last_temperature;
}

float getSensorHumidity() {
  return last_humidity;
}

bool areSensorsReady() {
  return sensors_initialized && last_reading_valid;
}

uint32_t getLastReadingTime() {
  return last_reading_time;
}

// ==== FONCTIONS PRIVÉES IMPLÉMENTATION ====

static bool validateSensorData(float temp, float hum) {
  // Vérification plage température (-40°C à +85°C pour SHT31)
  if (temp < -40.0f || temp > 85.0f) {
    return false;
  }
  
  // Vérification plage humidité (0% à 100%)
  if (hum < 0.0f || hum > 100.0f) {
    return false;
  }
  
  // Vérification cohérence (pas de changement brutal > 10°C/30s)
  if (last_reading_valid) {
    float temp_diff = abs(temp - last_temperature);
    if (temp_diff > 10.0f) {
      DEBUG_PRINTF("ATTENTION: Changement température brutal: %.1f°C\n", temp_diff);
      // On accepte quand même mais on log l'anomalie
    }
  }
  
  return true;
}