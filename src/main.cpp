#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>

#include "config.h"
#include "sensors.h"
#include "heating.h"
#include "display.h"
#include "weather.h"
#include "webserver.h"
#include "camera.h"
#include "storage.h"
#include "solar.h"

// ==== VARIABLES GLOBALES ====
SystemConfig config;
SystemState state;
HistoryPoint history[HISTORY_SIZE];
uint16_t history_index = 0;

// Timers
uint32_t last_main_loop = 0;
uint32_t last_oled_update = 0;
uint32_t last_weather_check = 0;

// NTP pour l'heure
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);  // UTC+1, update toutes les minutes

// ==== DÉCLARATIONS FONCTIONS ====
void initializeSystem();
void connectWiFi();
void mainControlLoop();
void updateHistory();
void calculateDailyAverages();
bool isTimeForWeatherUpdate();
void handleSecurityCheck();
void printSystemStatus();

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  DEBUG_PRINTLN("=== DÉMARRAGE SYSTÈME TAPIS CHAUFFANT ===");
  
  // Initialisation du système
  initializeSystem();
  
  DEBUG_PRINTLN("=== SYSTÈME PRÊT ===");
}

void loop() {
  uint32_t current_time = millis();
  
  // Boucle principale toutes les 30 secondes
  if (current_time - last_main_loop >= MAIN_LOOP_INTERVAL_MS) {
    mainControlLoop();
    last_main_loop = current_time;
  }
  
  // Mise à jour OLED
  if (current_time - last_oled_update >= OLED_PAGE_INTERVAL_MS) {
    updateOLED();
    last_oled_update = current_time;
  }
  
  // Vérification météo (si WiFi connecté)
  if (state.wifi_connected && isTimeForWeatherUpdate()) {
    updateWeatherData();
    last_weather_check = current_time;
  }
  
  // Gestion serveur web
  handleWebServer();
  
  // Mise à jour NTP si connecté
  if (state.wifi_connected) {
    timeClient.update();
  }
  
  // Petit délai pour éviter la surcharge CPU
  delay(100);
}

void initializeSystem() {
  DEBUG_PRINTLN("Initialisation matériel...");
  
  // Initialisation I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  DEBUG_PRINTLN("Bus I2C initialisé");
  
  // Chargement configuration
  loadConfiguration();
  DEBUG_PRINTLN("Configuration chargée");
  
  // Initialisation des modules
  initSensors();
  initHeating();
  initDisplay();
  initCamera();
  
  // État initial
  memset(&state, 0, sizeof(state));
  state.target_temp = config.temp_day;  // Température par défaut
  
  DEBUG_PRINTLN("Modules initialisés");
  
  // Connexion WiFi
  connectWiFi();
  
  // Initialisation serveur web
  if (state.wifi_connected) {
    initWebServer();
    timeClient.begin();
    DEBUG_PRINTLN("Serveur web démarré");
  }
  
  // Lecture initiale des capteurs
  readSensors();
  
  DEBUG_PRINTLN("Initialisation terminée");
}

void connectWiFi() {
  if (strlen(config.wifi_ssid) == 0) {
    DEBUG_PRINTLN("Pas de configuration WiFi - Mode autonome");
    state.wifi_connected = false;
    return;
  }
  
  DEBUG_PRINTF("Connexion WiFi: %s\n", config.wifi_ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid, config.wifi_password);
  
  int timeout = 20; // 20 secondes max
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    DEBUG_PRINT(".");
    timeout--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    state.wifi_connected = true;
    DEBUG_PRINTF("\nWiFi connecté! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    state.wifi_connected = false;
    DEBUG_PRINTLN("\nÉchec connexion WiFi - Mode autonome");
  }
}

void mainControlLoop() {
  DEBUG_PRINTLN("=== BOUCLE PRINCIPALE ===");
  
  // 1. Lecture capteurs
  bool sensor_ok = readSensors();
  if (sensor_ok) {
    state.current_temp = getSensorTemperature();
    state.current_humidity = getSensorHumidity();
    state.last_sensor_read = millis();
    
    DEBUG_PRINTF("Température: %.1f°C, Humidité: %.1f%%\n", 
                 state.current_temp, state.current_humidity);
  } else {
    DEBUG_PRINTLN("ERREUR: Lecture capteurs échouée");
    return; // Pas de régulation si pas de température
  }
  
  // 2. Mise à jour historique
  updateHistory();
  
  // 3. Calcul moyennes 24h
  calculateDailyAverages();
  
  // 4. Détermination mode jour/nuit
  uint32_t current_hour = 0;
  if (state.wifi_connected && timeClient.isTimeSet()) {
    current_hour = timeClient.getHours();
    // Calcul astronomique si coordonnées valides
    if (config.latitude != 0 && config.longitude != 0) {
      updateSolarTimes();
      state.is_day_mode = isDayTime();
    } else {
      // Mode fixe 8h-20h
      state.is_day_mode = (current_hour >= DEFAULT_DAY_START_HOUR && 
                          current_hour < DEFAULT_NIGHT_START_HOUR);
    }
  } else {
    // Mode autonome - utiliser heure système approximative
    uint32_t hours_since_boot = (millis() / 3600000) % 24;
    state.is_day_mode = (hours_since_boot >= DEFAULT_DAY_START_HOUR && 
                        hours_since_boot < DEFAULT_NIGHT_START_HOUR);
  }
  
  // 5. Calcul consigne température
  switch (config.target_mode) {
    case TARGET_DAY_NIGHT:
      state.target_temp = state.is_day_mode ? config.temp_day : config.temp_night;
      break;
      
    case TARGET_WEATHER:
      if (state.wifi_connected && state.weather_temp > 0) {
        state.target_temp = state.weather_temp;
      } else {
        // Fallback sur mode jour/nuit si pas de météo
        state.target_temp = state.is_day_mode ? config.temp_day : config.temp_night;
      }
      break;
  }
  
  DEBUG_PRINTF("Mode: %s, Consigne: %.1f°C\n", 
               state.is_day_mode ? "JOUR" : "NUIT", state.target_temp);
  
  // 6. Vérification sécurité
  handleSecurityCheck();
  
  // 7. Régulation chauffage (sauf si sécurité activée)
  if (!state.security_mode) {
    switch (config.heating_mode) {
      case HEATING_PID:
        state.heating_on = updatePIDControl(state.current_temp, state.target_temp);
        break;
        
      case HEATING_HYSTERESIS:
        state.heating_on = updateHysteresisControl(state.current_temp, state.target_temp);
        break;
    }
  } else {
    state.heating_on = false;
  }
  
  // 8. Application commande chauffage
  setHeatingOutput(state.heating_on);
  
  DEBUG_PRINTF("Chauffage: %s%s\n", 
               state.heating_on ? "ON" : "OFF",
               state.security_mode ? " (SÉCURITÉ)" : "");
  
  // 9. Status complet si debug
  if (DEBUG_ENABLED) {
    printSystemStatus();
  }
}

void updateHistory() {
  // Ajout point dans historique circulaire
  history[history_index].timestamp = millis();
  history[history_index].temperature = state.current_temp;
  history[history_index].humidity = state.current_humidity;
  history[history_index].heating_on = state.heating_on;
  
  history_index = (history_index + 1) % HISTORY_SIZE;
  
  DEBUG_PRINTF("Historique mis à jour [%d/%d]\n", history_index, HISTORY_SIZE);
}

void calculateDailyAverages() {
  float temp_sum = 0;
  float humidity_sum = 0;
  uint16_t count = 0;
  
  // Parcours historique pour calcul moyenne
  for (int i = 0; i < HISTORY_SIZE; i++) {
    if (history[i].timestamp > 0) {  // Point valide
      temp_sum += history[i].temperature;
      humidity_sum += history[i].humidity;
      count++;
    }
  }
  
  if (count > 0) {
    state.temp_avg_24h = temp_sum / count;
    state.humidity_avg_24h = humidity_sum / count;
    
    DEBUG_PRINTF("Moyennes 24h - Temp: %.1f°C, Humidité: %.1f%% (%d points)\n",
                 state.temp_avg_24h, state.humidity_avg_24h, count);
  }
}

bool isTimeForWeatherUpdate() {
  return (millis() - state.last_weather_update) >= WEATHER_UPDATE_INTERVAL_MS;
}

void handleSecurityCheck() {
  if (state.current_temp >= MAX_TEMP_SECURITY) {
    if (!state.security_mode) {
      state.security_mode = true;
      DEBUG_PRINTLN("ALERTE SÉCURITÉ: Température dépassée - Chauffage forcé OFF");
    }
  } else if (state.current_temp < (MAX_TEMP_SECURITY - 2.0f)) {
    // Hystérésis 2°C pour éviter oscillations
    if (state.security_mode) {
      state.security_mode = false;
      DEBUG_PRINTLN("Sécurité désactivée - Température revenue normale");
    }
  }
}

void printSystemStatus() {
  DEBUG_PRINTLN("--- STATUS SYSTÈME ---");
  DEBUG_PRINTF("Température actuelle: %.1f°C\n", state.current_temp);
  DEBUG_PRINTF("Humidité: %.1f%%\n", state.current_humidity);
  DEBUG_PRINTF("Consigne: %.1f°C\n", state.target_temp);
  DEBUG_PRINTF("Mode: %s\n", state.is_day_mode ? "JOUR" : "NUIT");
  DEBUG_PRINTF("Chauffage: %s\n", state.heating_on ? "ON" : "OFF");
  DEBUG_PRINTF("Sécurité: %s\n", state.security_mode ? "ACTIVE" : "OK");
  DEBUG_PRINTF("WiFi: %s\n", state.wifi_connected ? "CONNECTÉ" : "DÉCONNECTÉ");
  DEBUG_PRINTF("Mémoire libre: %d bytes\n", ESP.getFreeHeap());
  DEBUG_PRINTLN("---------------------");
}