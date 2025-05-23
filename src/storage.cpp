#include "storage.h"
#include "config.h"
#include <Preferences.h>

// Variables globales externes
extern SystemConfig config;
extern HistoryPoint history[HISTORY_SIZE];
extern uint16_t history_index;

// ==== VARIABLES PRIVÉES ====
static Preferences prefs;
static bool storage_initialized = false;

// Clés de stockage
#define PREFS_NAMESPACE "antHeater"
#define KEY_CONFIG_VERSION "cfg_ver"
#define KEY_HEATING_MODE "heat_mode"
#define KEY_TARGET_MODE "tgt_mode"
#define KEY_PID_KP "pid_kp"
#define KEY_PID_KI "pid_ki"
#define KEY_PID_KD "pid_kd"
#define KEY_HYST_TARGET "hyst_tgt"
#define KEY_TEMP_DAY "temp_day"
#define KEY_TEMP_NIGHT "temp_night"
#define KEY_LATITUDE "latitude"
#define KEY_LONGITUDE "longitude"
#define KEY_LED_INTENSITY "led_int"
#define KEY_LED_COLOR "led_color"
#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PASS "wifi_pass"
#define KEY_FIRST_BOOT "first_boot"
#define KEY_HISTORY_INDEX "hist_idx"
#define KEY_HISTORY_DATA "hist_data"

#define CONFIG_VERSION 1

void initStorage() {
  DEBUG_PRINTLN("Initialisation système de stockage...");
  
  storage_initialized = prefs.begin(PREFS_NAMESPACE, false);
  
  if (storage_initialized) {
    DEBUG_PRINTLN("Stockage initialisé avec succès");
    DEBUG_PRINTF("Espace utilisé: %zu bytes\n", getStorageUsage());
  } else {
    DEBUG_PRINTLN("ERREUR: Impossible d'initialiser le stockage");
  }
}

bool loadConfiguration() {
  if (!storage_initialized) {
    DEBUG_PRINTLN("Stockage non initialisé - Utilisation valeurs par défaut");
    resetConfigurationToDefaults();
    return false;
  }
  
  DEBUG_PRINTLN("Chargement configuration...");
  
  // Vérification version de configuration
  uint8_t stored_version = prefs.getUChar(KEY_CONFIG_VERSION, 0);
  if (stored_version != CONFIG_VERSION) {
    DEBUG_PRINTF("Version config différente (%d vs %d) - Reset\n", 
                 stored_version, CONFIG_VERSION);
    resetConfigurationToDefaults();
    return false;
  }
  
  // Chargement des paramètres
  config.heating_mode = (HeatingMode)prefs.getUChar(KEY_HEATING_MODE, HEATING_PID);
  config.target_mode = (TargetMode)prefs.getUChar(KEY_TARGET_MODE, TARGET_DAY_NIGHT);
  
  config.pid_kp = prefs.getFloat(KEY_PID_KP, DEFAULT_PID_KP);
  config.pid_ki = prefs.getFloat(KEY_PID_KI, DEFAULT_PID_KI);
  config.pid_kd = prefs.getFloat(KEY_PID_KD, DEFAULT_PID_KD);
  
  config.hysteresis_target = prefs.getFloat(KEY_HYST_TARGET, DEFAULT_TEMP_DAY);
  
  config.temp_day = prefs.getFloat(KEY_TEMP_DAY, DEFAULT_TEMP_DAY);
  config.temp_night = prefs.getFloat(KEY_TEMP_NIGHT, DEFAULT_TEMP_NIGHT);
  
  config.latitude = prefs.getFloat(KEY_LATITUDE, DEFAULT_LATITUDE);
  config.longitude = prefs.getFloat(KEY_LONGITUDE, DEFAULT_LONGITUDE);
  
  config.led_intensity = prefs.getUChar(KEY_LED_INTENSITY, DEFAULT_LED_INTENSITY);
  config.led_color = (FlashColor)prefs.getUChar(KEY_LED_COLOR, DEFAULT_FLASH_COLOR);
  
  // Chargement WiFi
  prefs.getString(KEY_WIFI_SSID, config.wifi_ssid, sizeof(config.wifi_ssid));
  prefs.getString(KEY_WIFI_PASS, config.wifi_password, sizeof(config.wifi_password));
  
  DEBUG_PRINTLN("Configuration chargée:");
  DEBUG_PRINTF("  Mode chauffage: %d\n", config.heating_mode);
  DEBUG_PRINTF("  Mode consigne: %d\n", config.target_mode);
  DEBUG_PRINTF("  PID: Kp=%.1f Ki=%.1f Kd=%.1f\n", config.pid_kp, config.pid_ki, config.pid_kd);
  DEBUG_PRINTF("  Températures: Jour=%.1f Nuit=%.1f\n", config.temp_day, config.temp_night);
  DEBUG_PRINTF("  Coordonnées: %.2f,%.2f\n", config.latitude, config.longitude);
  DEBUG_PRINTF("  WiFi SSID: %s\n", strlen(config.wifi_ssid) > 0 ? config.wifi_ssid : "(vide)");
  
  return true;
}

bool saveConfiguration() {
  if (!storage_initialized) {
    DEBUG_PRINTLN("ERREUR: Impossible de sauvegarder - Stockage non initialisé");
    return false;
  }
  
  DEBUG_PRINTLN("Sauvegarde configuration...");
  
  // Version de configuration
  prefs.putUChar(KEY_CONFIG_VERSION, CONFIG_VERSION);
  
  // Modes
  prefs.putUChar(KEY_HEATING_MODE, config.heating_mode);
  prefs.putUChar(KEY_TARGET_MODE, config.target_mode);
  
  // PID
  prefs.putFloat(KEY_PID_KP, config.pid_kp);
  prefs.putFloat(KEY_PID_KI, config.pid_ki);
  prefs.putFloat(KEY_PID_KD, config.pid_kd);
  
  // Hystérésis
  prefs.putFloat(KEY_HYST_TARGET, config.hysteresis_target);
  
  // Températures jour/nuit
  prefs.putFloat(KEY_TEMP_DAY, config.temp_day);
  prefs.putFloat(KEY_TEMP_NIGHT, config.temp_night);
  
  // Coordonnées
  prefs.putFloat(KEY_LATITUDE, config.latitude);
  prefs.putFloat(KEY_LONGITUDE, config.longitude);
  
  // LED
  prefs.putUChar(KEY_LED_INTENSITY, config.led_intensity);
  prefs.putUChar(KEY_LED_COLOR, config.led_color);
  
  // WiFi
  prefs.putString(KEY_WIFI_SSID, config.wifi_ssid);
  prefs.putString(KEY_WIFI_PASS, config.wifi_password);
  
  // Marquer comme configuré
  prefs.putBool(KEY_FIRST_BOOT, false);
  
  DEBUG_PRINTLN("Configuration sauvegardée avec succès");
  return true;
}

void resetConfigurationToDefaults() {
  DEBUG_PRINTLN("Initialisation configuration par défaut...");
  
  // Modes
  config.heating_mode = HEATING_PID;
  config.target_mode = TARGET_DAY_NIGHT;
  
  // PID
  config.pid_kp = DEFAULT_PID_KP;
  config.pid_ki = DEFAULT_PID_KI;
  config.pid_kd = DEFAULT_PID_KD;
  
  // Hystérésis
  config.hysteresis_target = DEFAULT_TEMP_DAY;
  
  // Températures
  config.temp_day = DEFAULT_TEMP_DAY;
  config.temp_night = DEFAULT_TEMP_NIGHT;
  
  // Coordonnées
  config.latitude = DEFAULT_LATITUDE;
  config.longitude = DEFAULT_LONGITUDE;
  
  // LED
  config.led_intensity = DEFAULT_LED_INTENSITY;
  config.led_color = DEFAULT_FLASH_COLOR;
  
  // WiFi vide
  memset(config.wifi_ssid, 0, sizeof(config.wifi_ssid));
  memset(config.wifi_password, 0, sizeof(config.wifi_password));
  
  DEBUG_PRINTLN("Configuration par défaut appliquée");
}

bool isFirstBoot() {
  if (!storage_initialized) return true;
  return prefs.getBool(KEY_FIRST_BOOT, true);
}

bool saveHistoryData() {
  if (!storage_initialized) return false;
  
  DEBUG_PRINTLN("Sauvegarde historique...");
  
  // Sauvegarde index
  prefs.putUShort(KEY_HISTORY_INDEX, history_index);
  
  // Sauvegarde données (attention à la taille!)
  // On ne sauvegarde que les derniers points pour éviter dépassement mémoire
  size_t data_size = sizeof(HistoryPoint) * HISTORY_SIZE;
  if (data_size > 495000) { // Limite approximative NVS ESP32
    DEBUG_PRINTF("ATTENTION: Historique trop volumineux (%zu bytes)\n", data_size);
    return false;
  }
  
  size_t written = prefs.putBytes(KEY_HISTORY_DATA, history, data_size);
  
  DEBUG_PRINTF("Historique sauvegardé: %zu bytes\n", written);
  return (written == data_size);
}

bool loadHistoryData() {
  if (!storage_initialized) return false;
  
  DEBUG_PRINTLN("Chargement historique...");
  
  // Chargement index
  history_index = prefs.getUShort(KEY_HISTORY_INDEX, 0);
  
  // Chargement données
  size_t data_size = sizeof(HistoryPoint) * HISTORY_SIZE;
  size_t read_size = prefs.getBytes(KEY_HISTORY_DATA, history, data_size);
  
  if (read_size == data_size) {
    DEBUG_PRINTF("Historique chargé: %zu bytes\n", read_size);
    return true;
  } else {
    DEBUG_PRINTF("Historique incomplet ou inexistant (%zu/%zu bytes)\n", read_size, data_size);
    // Initialisation historique vide
    memset(history, 0, data_size);
    history_index = 0;
    return false;
  }
}

void clearAllStoredData() {
  if (!storage_initialized) return;
  
  DEBUG_PRINTLN("Suppression de toutes les données stockées...");
  prefs.clear();
  DEBUG_PRINTLN("Données effacées");
}

size_t getStorageUsage() {
  if (!storage_initialized) return 0;
  return prefs.freeEntries(); // Approximation
}