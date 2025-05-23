#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_SHT31.h>
#include <Wire.h>
#include "config.h"

// Classe de gestion du capteur SHT31
class SensorManager {
private:
    Adafruit_SHT31 sht31;
    bool sensor_initialized;
    unsigned long last_read;
    const unsigned long READ_INTERVAL = 5000; // Lecture toutes les 5 secondes
    
    // Historique pour moyennes
    static const int HISTORY_SIZE = 288; // 24h avec lecture toutes les 5min
    float temp_history[HISTORY_SIZE];
    float humidity_history[HISTORY_SIZE];
    int history_index;
    bool history_full;
    
public:
    SensorManager();
    bool begin();
    bool readSensors();
    float getTemperature();
    float getHumidity();
    float getTemperatureAverage24h();
    float getHumidityAverage24h();
    bool isInitialized();
    void addToHistory();
};

extern SensorManager sensors;

#endif