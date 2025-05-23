#include "config.h"

// Variables globales
Config config;
bool wifi_connected = false;
unsigned long last_weather_update = 0;
float current_temp = 0.0;
float current_humidity = 0.0;
float target_temp = 25.0;
bool heating_on = false;
int current_page = 0;
unsigned long last_page_change = 0;

/**
 * Charge la configuration depuis SPIFFS
 */
bool loadConfig() {
    if (!SPIFFS.exists("/config.json")) {
        debugLog("Fichier config.json inexistant, utilisation config par défaut");
        saveConfig(); // Créer le fichier avec les valeurs par défaut
        return true;
    }
    
    File file = SPIFFS.open("/config.json", "r");
    if (!file) {
        debugLog("Erreur ouverture config.json");
        return false;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        debugLog("Erreur parsing JSON: " + String(error.c_str()));
        return false;
    }
    
    // Chargement des paramètres
    config.mode_chauffage = doc["mode_chauffage"] | 0;
    config.mode_consigne = doc["mode_consigne"] | 0;
    config.pid_kp = doc["pid_kp"] | 2.0;
    config.pid_ki = doc["pid_ki"] | 0.5;
    config.pid_kd = doc["pid_kd"] | 1.0;
    config.hyst_ecart = doc["hyst_ecart"] | 0.2;
    config.temp_jour = doc["temp_jour"] | 28.0;
    config.temp_nuit = doc["temp_nuit"] | 24.0;
    config.ecart_jn = doc["ecart_jn"] | 0.0;
    config.latitude = doc["latitude"] | 48.85;
    config.longitude = doc["longitude"] | 2.35;
    config.led_intensity = doc["led_intensity"] | 128;
    config.led_color = doc["led_color"] | 0;
    config.stream_active = doc["stream_active"] | 1;
    config.stream_qualite = doc["stream_qualite"] | 0;
    
    debugLog("Configuration chargée avec succès");
    return true;
}

/**
 * Sauvegarde la configuration dans SPIFFS
 */
bool saveConfig() {
    DynamicJsonDocument doc(2048);
    
    doc["mode_chauffage"] = config.mode_chauffage;
    doc["mode_consigne"] = config.mode_consigne;
    doc["pid_kp"] = config.pid_kp;
    doc["pid_ki"] = config.pid_ki;
    doc["pid_kd"] = config.pid_kd;
    doc["hyst_ecart"] = config.hyst_ecart;
    doc["temp_jour"] = config.temp_jour;
    doc["temp_nuit"] = config.temp_nuit;
    doc["ecart_jn"] = config.ecart_jn;
    doc["latitude"] = config.latitude;
    doc["longitude"] = config.longitude;
    doc["led_intensity"] = config.led_intensity;
    doc["led_color"] = config.led_color;
    doc["stream_active"] = config.stream_active;
    doc["stream_qualite"] = config.stream_qualite;
    
    File file = SPIFFS.open("/config.json", "w");
    if (!file) {
        debugLog("Erreur création config.json");
        return false;
    }
    
    serializeJson(doc, file);
    file.close();
    
    debugLog("Configuration sauvegardée");
    return true;
}

/**
 * Remet la configuration aux valeurs par défaut
 */
void resetConfig() {
    config = Config(); // Utilise les valeurs par défaut du constructeur
    saveConfig();
    debugLog("Configuration remise à zéro");
}

/**
 * Affiche un message de debug si activé
 */
void debugLog(const String& message) {
    if (config.debug_enabled) {
        Serial.println("[DEBUG] " + message);
    }
}