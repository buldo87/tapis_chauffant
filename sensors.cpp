#include "sensors.h"

SensorManager sensors;

/**
 * Constructeur
 */
SensorManager::SensorManager() {
    sensor_initialized = false;
    last_read = 0;
    history_index = 0;
    history_full = false;
    
    // Initialisation de l'historique
    for (int i = 0; i < HISTORY_SIZE; i++) {
        temp_history[i] = 0.0;
        humidity_history[i] = 0.0;
    }
}

/**
 * Initialise le capteur SHT31
 */
bool SensorManager::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    
    if (!sht31.begin(SHT31_ADDRESS)) {
        debugLog("Erreur: SHT31 non détecté à l'adresse 0x" + String(SHT31_ADDRESS, HEX));
        sensor_initialized = false;
        return false;
    }
    
    debugLog("SHT31 initialisé avec succès");
    sensor_initialized = true;
    
    // Première lecture
    readSensors();
    
    return true;
}

/**
 * Lit les données du capteur
 */
bool SensorManager::readSensors() {
    if (!sensor_initialized) {
        return false;
    }
    
    unsigned long now = millis();
    if (now - last_read < READ_INTERVAL) {
        return true; // Pas encore temps de relire
    }
    
    float temp = sht31.readTemperature();
    float hum = sht31.readHumidity();
    
    // Vérification validité des données
    if (isnan(temp) || isnan(hum)) {
        debugLog("Erreur lecture SHT31: données invalides");
        return false;
    }
    
    // Vérification cohérence des données
    if (temp < -40 || temp > 85 || hum < 0 || hum > 100) {
        debugLog("Erreur lecture SHT31: valeurs hors limites");
        return false;
    }
    
    current_temp = temp;
    current_humidity = hum;
    last_read = now;
    
    debugLog("Température: " + String(temp, 1) + "°C, Humidité: " + String(hum, 1) + "%");
    
    return true;
}

/**
 * Retourne la température actuelle
 */
float SensorManager::getTemperature() {
    return current_temp;
}

/**
 * Retourne l'humidité actuelle
 */
float SensorManager::getHumidity() {
    return current_humidity;
}

/**
 * Ajoute les valeurs actuelles à l'historique
 */
void SensorManager::addToHistory() {
    temp_history[history_index] = current_temp;
    humidity_history[history_index] = current_humidity;
    
    history_index++;
    if (history_index >= HISTORY_SIZE) {
        history_index = 0;
        history_full = true;
    }
    
    debugLog("Données ajoutées à l'historique (index: " + String(history_index) + ")");
}

/**
 * Calcule la moyenne température sur 24h
 */
float SensorManager::getTemperatureAverage24h() {
    if (!history_full && history_index == 0) {
        return current_temp; // Pas assez de données
    }
    
    float sum = 0.0;
    int count = history_full ? HISTORY_SIZE : history_index;
    
    for (int i = 0; i < count; i++) {
        sum += temp_history[i];
    }
    
    return sum / count;
}

/**
 * Calcule la moyenne humidité sur 24h
 */
float SensorManager::getHumidityAverage24h() {
    if (!history_full && history_index == 0) {
        return current_humidity; // Pas assez de données
    }
    
    float sum = 0.0;
    int count = history_full ? HISTORY_SIZE : history_index;
    
    for (int i = 0; i < count; i++) {
        sum += humidity_history[i];
    }
    
    return sum / count;
}

/**
 * Vérifie si le capteur est initialisé
 */
bool SensorManager::isInitialized() {
    return sensor_initialized;
}