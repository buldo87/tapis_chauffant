#include "wifi_credentials.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Preferences.h>
#include <time.h>
#include <PID_v1.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_camera.h"
#include "logo.h"
#include "LittleFS.h"
#include "esp_task_wdt.h"

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5

#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

// Définition des broches I2C pour l'écran SSD1306
#define I2C_SDA 1
#define I2C_SCL 2

// Configuration de l'écran SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C  // Adresse I2C typique pour SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Configuration capteur 
Adafruit_SHT31 sht31 = Adafruit_SHT31();


// === Tapis Chauffant ===
#define HEATER_PIN 42//5

AsyncWebServer server(80);
Preferences preferences;

// === Variables globales ===
float internalTemp = NAN;
float internalHum = NAN;
float externalTemp = 0.0; 
float externalHum = 0.0;

bool heaterState = false;
float lastTemperature = 0.0;
bool cameraEnabled = false;
double setpoint, input, output;
double Kp = 2.0, Ki = 5.0, Kd = 1.0;
float hysteresis = 0.3;
const int windowSize = 1000;
float temperatureWindow[windowSize];
float humiditeWindow[windowSize];
int windowIndex = 0;
bool windowFilled = false;
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;
const float MAX_TEMP_VARIATION = 5.0;
const float MAX_HUMIDITY_VARIATION = 10.0;
float movingAverageTemp = -INFINITY;
float movingAverageHum = -INFINITY;
float maxTemperature = -INFINITY;
float minTemperature = INFINITY;
float maxHumidite = -INFINITY;
float minHumidite = INFINITY;
float globalMaxTempSet = 40;
float globalMinTempSet = 0;
unsigned long lastToggleTime = 0;
bool manualCycleOn = false;
bool usePWM = false;  // true = PID actif, false = tout ou rien
bool weatherModeEnabled = false;
// safe mode
enum SafetyLevel {
    SAFETY_NORMAL = 0,
    SAFETY_WARNING = 1,
    SAFETY_CRITICAL = 2,
    SAFETY_EMERGENCY = 3
};
struct SafetySystem {
    SafetyLevel currentLevel = SAFETY_NORMAL;
    unsigned long lastSensorRead = 0;
    unsigned long lastValidTemperature = 0;
    unsigned long lastValidHumidity = 0;
    unsigned long safetyActivatedTime = 0;
    int consecutiveFailures = 0;
    int temperatureOutOfRangeCount = 0;
    int humidityOutOfRangeCount = 0;
    bool emergencyShutdown = false;
    String lastErrorMessage = "";
    float lastKnownGoodTemp = 22.0;
    float lastKnownGoodHum = 50.0;
};
SafetySystem safety;
// Seuils de sécurité
const float TEMP_EMERGENCY_HIGH = 40.0;    // Température critique haute
const float TEMP_EMERGENCY_LOW = 10.0;     // Température critique basse
const float TEMP_WARNING_HIGH = 35.0;      // Température d'alerte haute
const float TEMP_WARNING_LOW = 15.0;       // Température d'alerte basse
const float HUM_EMERGENCY_HIGH = 95.0;     // Humidité critique haute
const float HUM_EMERGENCY_LOW = 10.0;      // Humidité critique basse
const unsigned long SENSOR_TIMEOUT = 30000; // 30 secondes sans lecture
const unsigned long SAFETY_RESET_DELAY = 300000; // 5 minutes avant tentative de reset
const int MAX_CONSECUTIVE_FAILURES = 3;    // Nombre d'échecs consécutifs

// === Déclaration des variables pour la courbe de température (à ajouter avec les autres variables globales) ===
const int TEMP_CURVE_POINTS = 24;  // Un point par heure
float tempCurve[TEMP_CURVE_POINTS]; // Températures pour chaque heure
bool useTempCurve = false;         // Activer/désactiver la courbe de température

float latitude = 48.85; // Paris par défaut
float longitude = 2.35;
int  DST_offset =   2;

unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 3600000; // 1h
// === Historique circulaire en RAM uniquement ===
const int MAX_HISTORY_RECORDS = 1440;

// === Affichage OLED ===
unsigned long lastDisplayChange = 0;
int displayPage = 0;
const int pageCount = 4;

// logo
int16_t positionImageAxeHorizontal = 20;     // Position de la gauche de l’image à 20 pixels du bord gauche de l’écran
int16_t positionImageAxeVertical = 0;       // Position du haut de l’image à 16 pixels du bord haut de l’écran OLED
int16_t largeurDeLimage = 64;                // Largeur de l’image à afficher : 64 pixels
int16_t hauteurDeLimage = 64;                // Hauteur de l’image à afficher : 64 pixels

struct YearlyWeatherData {
    float temperaturesByDayHour[366][24]; // 366 jours x 24 heures
    bool dataLoaded = false;
    int currentYear = 0;
    unsigned long lastUpdate = 0;
};
YearlyWeatherData yearlyData;

struct ExternalWeather {
  float temperature;
  float humidity;
};

struct HistoryRecord {
  time_t timestamp;
  float temperature;
  float humidity;
};

PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

HistoryRecord history[MAX_HISTORY_RECORDS];
int historyIndex = 0;
bool historyFull = false;

struct SystemConfig {
    bool usePWM = false;
    bool weatherModeEnabled = false;
    bool cameraEnabled = false;
    bool useTempCurve = false;
    float hysteresis = 0.3;
    float Kp = 2.0, Ki = 5.0, Kd = 1.0;
    float latitude = 48.85;
    float longitude = 2.35;
    float tempCurve[TEMP_CURVE_POINTS];
    float globalMinTempSet = 15.0;
    float globalMaxTempSet = 35.0;
    char lastSaveTime[20] = "never";
};
SystemConfig config;

void activateWarningMode(const String& reason) {
    // Mode d'alerte : continuer le fonctionnement mais avec surveillance renforcée
    Serial.println("⚠️ MODE ALERTE ACTIVÉ: " + reason);
    
    // Affichage OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ALERTE!");
    display.setCursor(0, 12);
    display.println(reason.substring(0, 21)); // Limite longueur
    display.setCursor(0, 24);
    display.printf("Temp: %.1fC", safety.lastKnownGoodTemp);
    display.setCursor(0, 36);
    display.printf("Hum: %.0f%%", safety.lastKnownGoodHum);
    display.setCursor(0, 48);
    display.println("Surveillance++");
    display.display();
    
    // Réduire la puissance du chauffage en mode alerte
    if (output > 128) {
        output = 128; // Limiter à 50%
        analogWrite(HEATER_PIN, output);
    }
}

void activateCriticalMode(const String& reason) {
    // Mode critique : fonctionnement en mode dégradé
    Serial.println("🔴 MODE CRITIQUE ACTIVÉ: " + reason);
    
    // Arrêter le chauffage
    output = 0;
    analogWrite(HEATER_PIN, 0);
    
    // Affichage OLED
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
    
    // Utiliser les dernières valeurs connues pour l'affichage web
    internalTemp = safety.lastKnownGoodTemp;
    internalHum = safety.lastKnownGoodHum;
}

void activateEmergencyMode(const String& reason) {
    // Mode d'urgence : arrêt complet du chauffage
    Serial.println("🚨 MODE URGENCE ACTIVÉ: " + reason);
    
    safety.emergencyShutdown = true;
    
    // Arrêt total du chauffage
    output = 0;
    analogWrite(HEATER_PIN, 0);
    
    // Affichage OLED en mode urgence (clignotant)
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
    display.println("URGENCE!");
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("ARRET COMPLET");
    display.setCursor(0, 32);
    display.println(reason.substring(0, 21));
    display.setCursor(0, 44);
    display.println("Verif. capteurs");
    display.display();
    
    // Optionnel: Son d'alarme si buzzer connecté
    // tone(BUZZER_PIN, 1000, 500);
}

void escalateSafety(SafetyLevel newLevel, const String& reason) {
    unsigned long now = millis();
    
    if (newLevel > safety.currentLevel) {
        safety.currentLevel = newLevel;
        safety.safetyActivatedTime = now;
        safety.lastErrorMessage = reason;
        
        Serial.printf("🚨 SÉCURITÉ NIVEAU %d: %s\n", newLevel, reason.c_str());
        
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

void exitSafeMode() {
    Serial.println("✅ RETOUR AU MODE NORMAL");
    
    safety.emergencyShutdown = false;
    safety.consecutiveFailures = 0;
    safety.temperatureOutOfRangeCount = 0;
    safety.humidityOutOfRangeCount = 0;
    safety.lastErrorMessage = "";
    
    // Affichage OLED normal
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SYSTEME OK");
    display.setCursor(0, 15);
    display.println("Reprise normale");
    display.setCursor(0, 30);
    display.printf("Temp: %.1fC", internalTemp);
    display.setCursor(0, 45);
    display.printf("Hum: %.0f%%", internalHum);
    display.display();
    
    delay(2000); // Afficher le message 2 secondes
}

void downgradeSafety() {
    SafetyLevel oldLevel = safety.currentLevel;
    
    if (safety.currentLevel > SAFETY_NORMAL) {
        safety.currentLevel = (SafetyLevel)(safety.currentLevel - 1);
        
        Serial.printf("✅ SÉCURITÉ: Niveau réduit de %d à %d\n", oldLevel, safety.currentLevel);
        
        if (safety.currentLevel == SAFETY_NORMAL) {
            exitSafeMode();
        }
    }
}

void checkSafetyConditions() {
    unsigned long now = millis();
    
    // 1. Vérifier si les capteurs répondent
    if (now - safety.lastSensorRead > SENSOR_TIMEOUT) {
        escalateSafety(SAFETY_CRITICAL, "Capteurs non réactifs depuis " + 
                      String((now - safety.lastSensorRead)/1000) + "s");
        return;
    }
    
    // 2. Vérifier les valeurs de température
    if (!isnan(internalTemp)) {
        safety.lastValidTemperature = now;
        safety.lastKnownGoodTemp = internalTemp;
        
        if (internalTemp >= TEMP_EMERGENCY_HIGH || internalTemp <= TEMP_EMERGENCY_LOW) {
            escalateSafety(SAFETY_EMERGENCY, "Température critique: " + String(internalTemp) + "°C");
            return;
        } else if (internalTemp >= TEMP_WARNING_HIGH || internalTemp <= TEMP_WARNING_LOW) {
            escalateSafety(SAFETY_WARNING, "Température d'alerte: " + String(internalTemp) + "°C");
        } else {
            safety.temperatureOutOfRangeCount = 0; // Reset si OK
        }
    } else {
        safety.consecutiveFailures++;
        if (safety.consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
            escalateSafety(SAFETY_CRITICAL, "Échecs consécutifs de lecture température: " + 
                          String(safety.consecutiveFailures));
            return;
        }
    }
    
    // 3. Vérifier les valeurs d'humidité
    if (!isnan(internalHum)) {
        safety.lastValidHumidity = now;
        safety.lastKnownGoodHum = internalHum;
        
        if (internalHum >= HUM_EMERGENCY_HIGH || internalHum <= HUM_EMERGENCY_LOW) {
            escalateSafety(SAFETY_WARNING, "Humidité critique: " + String(internalHum) + "%");
        }
    }
    
    // 4. Vérifier la cohérence des données
    if (abs(internalTemp - safety.lastKnownGoodTemp) > 10.0) {
        safety.temperatureOutOfRangeCount++;
        if (safety.temperatureOutOfRangeCount >= 3) {
            escalateSafety(SAFETY_WARNING, "Variation température suspecte: " + 
                          String(internalTemp) + "°C vs " + String(safety.lastKnownGoodTemp) + "°C");
        }
    }
    
    // 5. Vérifier l'état du chauffage
    if (output > 200 && internalTemp > setpoint + 2.0) {
        escalateSafety(SAFETY_WARNING, "Chauffage actif mais température élevée");
    }
    
    // 6. Si tout va bien, réduire le niveau de sécurité progressivement
    if (safety.currentLevel > SAFETY_NORMAL && 
        now - safety.safetyActivatedTime > SAFETY_RESET_DELAY) {
        
        if (internalTemp > TEMP_WARNING_LOW && internalTemp < TEMP_WARNING_HIGH &&
            internalHum > HUM_EMERGENCY_LOW && internalHum < HUM_EMERGENCY_HIGH) {
            
            downgradeSafety();
        }
    }
}

void saveCompleteConfig() {
    preferences.begin("system", false);
    
    // Sauver la structure complète
    preferences.putBytes("config", &config, sizeof(config));
    
    // Timestamp de sauvegarde
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        strftime(config.lastSaveTime, sizeof(config.lastSaveTime), 
                "%Y-%m-%d %H:%M:%S", &timeinfo);
        preferences.putString("lastSave", config.lastSaveTime);
    }
    
    preferences.end();
    Serial.printf("✅ Configuration complète sauvegardée à %s\n", config.lastSaveTime);
}

void loadCompleteConfig() {
    preferences.begin("system", true);
    
    // Charger la configuration ou utiliser les valeurs par défaut
    size_t configSize = preferences.getBytesLength("config");
    if (configSize == sizeof(config)) {
        preferences.getBytes("config", &config, sizeof(config));
        Serial.println("✅ Configuration restaurée depuis la mémoire");
    } else {
        Serial.println("⚠️ Aucune configuration trouvée, utilisation des valeurs par défaut");
        // Initialiser tempCurve avec des valeurs par défaut
        for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
            config.tempCurve[i] = 22.0 + (i >= 8 && i <= 20 ? 3.0 : 0.0); // Jour: 25°C, Nuit: 22°C
        }
    }
    
    // Appliquer les valeurs chargées
    usePWM = config.usePWM;
    weatherModeEnabled = config.weatherModeEnabled;
    cameraEnabled = config.cameraEnabled;
    useTempCurve = config.useTempCurve;
    hysteresis = config.hysteresis;
    Kp = config.Kp; Ki = config.Ki; Kd = config.Kd;
    latitude = config.latitude; longitude = config.longitude;
    memcpy(tempCurve, config.tempCurve, sizeof(tempCurve));
    
    String lastSave = preferences.getString("lastSave", "jamais");
    preferences.end();
    
    Serial.printf("📅 Dernière sauvegarde: %s\n", lastSave.c_str());
}

// Fonction à appeler avant chaque modification
void updateConfigAndSave() {
    config.usePWM = usePWM;
    config.weatherModeEnabled = weatherModeEnabled;
    config.cameraEnabled = cameraEnabled;
    config.useTempCurve = useTempCurve;
    config.hysteresis = hysteresis;
    config.Kp = Kp; config.Ki = Ki; config.Kd = Kd;
    config.latitude = latitude; config.longitude = longitude;
    memcpy(config.tempCurve, tempCurve, sizeof(tempCurve));
    
    saveCompleteConfig();
}

ExternalWeather getWeatherData() {
  ExternalWeather result = {NAN, NAN};

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) +
               "&longitude=" + String(longitude, 4) +
               "&current_weather=true&hourly=relative_humidity_2m";

  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("❌ Erreur API météo (%d) \n", httpCode);
    http.end();
    return result;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("❌ Erreur JSON météo : \n");
    Serial.println(error.c_str());
    return result;
  }

  result.temperature = doc["current_weather"]["temperature"] | NAN;

  String nowStr = doc["current_weather"]["time"];
  String targetHour = nowStr.substring(0, 14) + "00";

  JsonArray timeArr = doc["hourly"]["time"];
  JsonArray humArr = doc["hourly"]["relative_humidity_2m"];
  bool found = false;

  //Serial.printf("🔎 Recherche humidité pour %s \n", targetHour.c_str());
  for (size_t i = 0; i < timeArr.size(); i++) {
    if (timeArr[i].as<String>() == targetHour) {
      result.humidity = humArr[i].as<float>();
      found = true;
      break;
    }
  }

  if (!found) {
    Serial.println("❗ Aucune humidité trouvée pour l'heure demandée");
  }

  //Serial.printf("🌤️ Météo ext. : %.1f°C, 💧 %s%%  \n", result.temperature,
   //             isnan(result.humidity) ? "??" : String(result.humidity, 0).c_str());

  return result;
}

void addToHistory(float temperature, float humidity) {
  time_t now = time(nullptr);
  history[historyIndex] = { now, temperature, humidity };
  historyIndex = (historyIndex + 1) % MAX_HISTORY_RECORDS;
  if (historyIndex == 0) historyFull = true;
}

void handleHistoryRequest(AsyncWebServerRequest *request) {
  String json = "[";
  int total = historyFull ? MAX_HISTORY_RECORDS : historyIndex;
  bool first = true;

  for (int i = 0; i < total; i++) {
    int idx = (historyIndex + i) % MAX_HISTORY_RECORDS;
    HistoryRecord r = history[idx];

    if (r.timestamp == 0) continue; // 🚫 ignorer valeurs par défaut

    if (!first) json += ",";
    first = false;

    json += String("{\"t\":") + r.timestamp +
            ",\"temp\":" + String(r.temperature, 1) +
            ",\"hum\":" + String(r.humidity, 1) + "}";
  }

  json += "]";
  request->send(200, "application/json", json);
}

// Fonctions utilitaires pour parser les dates
int extractDayOfYear(const String& dateTimeStr) {
    // Format attendu: "2024-03-15T14:00"
    int year = dateTimeStr.substring(0, 4).toInt();
    int month = dateTimeStr.substring(5, 7).toInt();
    int day = dateTimeStr.substring(8, 10).toInt();
    
    struct tm tm_date = {0};
    tm_date.tm_year = year - 1900;
    tm_date.tm_mon = month - 1;
    tm_date.tm_mday = day;
    
    mktime(&tm_date);
    return tm_date.tm_yday;
}

int extractHour(const String& dateTimeStr) {
    // Format attendu: "2024-03-15T14:00"
    return dateTimeStr.substring(11, 13).toInt();
}

void logTemperatureEvent(float temp, float setpoint, int heaterOutput, const char* mode) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.printf("[%02d:%02d:%02d] T=%.1f°C, Cible=%.1f°C, Chauf=%d%%, Mode=%s\n",
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                     temp, setpoint, heaterOutput, mode);
    }
    /*
    // Optionnel: sauvegarder en fichier sur LittleFS
    File logFile = LittleFS.open("/temperature.log", "a");
    if (logFile) {
        char logLine[128];
        snprintf(logLine, sizeof(logLine), 
                "%02d:%02d:%02d,%.1f,%.1f,%d,%s\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                temp, setpoint, heaterOutput, mode);
        logFile.print(logLine);
        logFile.close();
    }
   */ 
}

bool loadWeatherChunk(int year, int startDay, int endDay) {
    // Calculer les dates
    struct tm startDate = {0};
    startDate.tm_year = year - 1900;
    startDate.tm_yday = startDay;
    mktime(&startDate);
    
    struct tm endDate = {0};
    endDate.tm_year = year - 1900;
    endDate.tm_yday = endDay;
    mktime(&endDate);
    
    char startStr[11], endStr[11];
    strftime(startStr, sizeof(startStr), "%Y-%m-%d", &startDate);
    strftime(endStr, sizeof(endStr), "%Y-%m-%d", &endDate);
    
    HTTPClient http;
    String url = "https://archive-api.open-meteo.com/v1/archive?latitude=" + 
                 String(latitude, 4) + "&longitude=" + String(longitude, 4) + 
                 "&start_date=" + startStr + "&end_date=" + endStr + 
                 "&hourly=temperature_2m&timezone=auto";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(32768); // Plus gros buffer
        
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {
            JsonArray temps = doc["hourly"]["temperature_2m"];
            JsonArray times = doc["hourly"]["time"];
            
            for (int i = 0; i < temps.size(); i++) {
                // Parser la date/heure et stocker la température
                String timeStr = times[i].as<String>();
                int day = extractDayOfYear(timeStr);
                int hour = extractHour(timeStr);
                
                if (day >= 0 && day < 366 && hour >= 0 && hour < 24) {
                    yearlyData.temperaturesByDayHour[day][hour] = temps[i].as<float>();
                }
            }
        }
    }
    
    http.end();
    Serial.printf("📅 Chunk %d-%d traité\n", startDay, endDay);
    return true;
}

void saveYearlyDataToFlash() {
    if (!yearlyData.dataLoaded) {
        Serial.println("⚠️ Aucune donnée annuelle à sauvegarder");
        return;
    }
    
    String filename = "/weather_" + String(yearlyData.currentYear) + ".json";
    File file = LittleFS.open(filename, "w");
    
    if (!file) {
        Serial.println("❌ Impossible d'ouvrir le fichier pour écriture: " + filename);
        return;
    }
    
    Serial.println("💾 Sauvegarde des données annuelles...");
    
    // Créer le JSON avec les métadonnées
    DynamicJsonDocument doc(200000); // ~200KB pour toutes les données
    doc["year"] = yearlyData.currentYear;
    doc["latitude"] = latitude;
    doc["longitude"] = longitude;
    doc["lastUpdate"] = yearlyData.lastUpdate;
    doc["dataLoaded"] = yearlyData.dataLoaded;
    
    // Sauvegarder les températures jour par jour
    JsonArray days = doc.createNestedArray("days");
    
    for (int day = 0; day < 366; day++) {
        JsonArray hours = days.createNestedArray();
        bool hasData = false;
        
        for (int hour = 0; hour < 24; hour++) {
            float temp = yearlyData.temperaturesByDayHour[day][hour];
            if (!isnan(temp) && temp != 0.0) {
                hasData = true;
            }
            //hours.add(isnan(temp) ? nullptr : temp);
            if (isnan(temp)){
                hours.add(nullptr);}
            else {
                hours.add(temp);
            }

        }
        
        // Si aucune donnée pour ce jour, ajouter un tableau vide
        if (!hasData) {
            hours.clear();
            for (int h = 0; h < 24; h++) {
                hours.add(nullptr);
            }
        }
    }
    
    // Écrire le JSON dans le fichier
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    if (bytesWritten > 0) {
        Serial.printf("✅ Données sauvegardées: %s (%zu octets)\n", 
                     filename.c_str(), bytesWritten);
        
        // Afficher les statistiques
        File info = LittleFS.open(filename, "r");
        if (info) {
            Serial.printf("📊 Taille du fichier: %zu octets\n", info.size());
            info.close();
        }
    } else {
        Serial.println("❌ Échec de la sauvegarde");
    }
}

bool loadYearlyDataFromFlash(int year) {
    String filename = "/weather_" + String(year) + ".json";
    
    if (!LittleFS.exists(filename)) {
        Serial.println("📁 Aucun fichier météo trouvé pour " + String(year));
        return false;
    }
    
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.println("❌ Impossible d'ouvrir: " + filename);
        return false;
    }
    
    Serial.println("📖 Chargement des données depuis: " + filename);
    
    // Parser le JSON
    DynamicJsonDocument doc(200000);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("❌ Erreur parsing JSON: " + String(error.c_str()));
        return false;
    }
    
    // Vérifier la cohérence des coordonnées
    float savedLat = doc["latitude"] | 0.0;
    float savedLon = doc["longitude"] | 0.0;
    
    if (abs(savedLat - latitude) > 0.1 || abs(savedLon - longitude) > 0.1) {
        Serial.printf("⚠️ Coordonnées différentes dans le fichier (%.2f,%.2f vs %.2f,%.2f)\n",
                     savedLat, savedLon, latitude, longitude);
        Serial.println("🗑️ Suppression du fichier obsolète");
        LittleFS.remove(filename);
        return false;
    }
    
    // Charger les données
    yearlyData.currentYear = doc["year"] | year;
    yearlyData.lastUpdate = doc["lastUpdate"] | 0;
    yearlyData.dataLoaded = doc["dataLoaded"] | false;
    
    JsonArray days = doc["days"];
    int loadedDays = 0;
    int loadedHours = 0;
    
    for (int day = 0; day < min(366, (int)days.size()); day++) {
        JsonArray hours = days[day];
        bool dayHasData = false;
        
        for (int hour = 0; hour < min(24, (int)hours.size()); hour++) {
            if (hours[hour].isNull()) {
                yearlyData.temperaturesByDayHour[day][hour] = NAN;
            } else {
                yearlyData.temperaturesByDayHour[day][hour] = hours[hour].as<float>();
                dayHasData = true;
                loadedHours++;
            }
        }
        
        if (dayHasData) loadedDays++;
    }
    
    Serial.printf("✅ Données chargées: %d jours, %d heures de données\n", 
                 loadedDays, loadedHours);
    
    return loadedHours > 0;
}

// Fonction pour nettoyer les vieux fichiers météo
void cleanupOldWeatherFiles() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    
    int currentYear = timeinfo.tm_year + 1900;
    
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    
    while (file) {
        String fileName = file.name();
        
        // Vérifier si c'est un fichier météo
        if (fileName.startsWith("weather_") && fileName.endsWith(".json")) {
            // Extraire l'année du nom de fichier
            int fileYear = fileName.substring(8, fileName.length() - 5).toInt();
            
            // Supprimer les fichiers de plus de 2 ans
            if (fileYear < currentYear - 2) {
                Serial.println("🗑️ Suppression ancien fichier: " + fileName);
                LittleFS.remove("/" + fileName);
            }
        }
        
        file = root.openNextFile();
    }
}

// Fonction pour obtenir des statistiques sur les fichiers stockés
void printWeatherFilesInfo() {
    Serial.println("📊 === Fichiers météo stockés ===");
    
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    size_t totalSize = 0;
    int fileCount = 0;
    
    while (file) {
        String fileName = file.name();
        
        if (fileName.startsWith("weather_") && fileName.endsWith(".json")) {
            size_t fileSize = file.size();
            totalSize += fileSize;
            fileCount++;
            
            Serial.printf("📁 %s - %zu octets\n", fileName.c_str(), fileSize);
        }
        
        file = root.openNextFile();
    }
    
    Serial.printf("📊 Total: %d fichiers, %zu octets\n", fileCount, totalSize);
    
    // Afficher l'espace libre
    Serial.printf("💾 Espace libre LittleFS: %zu octets\n", 
                 LittleFS.totalBytes() - LittleFS.usedBytes());
}

void loadYearlyWeatherData() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("❌ Impossible d'obtenir l'heure pour les données météo");
        return;
    }
    
    int currentYear = timeinfo.tm_year + 1900;
    
    // Nettoyer les vieux fichiers au démarrage
    static bool cleanupDone = false;
    if (!cleanupDone) {
        cleanupOldWeatherFiles();
        printWeatherFilesInfo();
        cleanupDone = true;
    }
    
    // Essayer de charger depuis le cache d'abord
    if (loadYearlyDataFromFlash(currentYear)) {
        Serial.println("✅ Données météo chargées depuis le cache");
        return;
    }
    
    // Si pas de cache ou données obsolètes, télécharger
    Serial.printf("🌐 Téléchargement nouvelles données météo pour %d...\n", currentYear);
    
    // Initialiser la structure
    yearlyData.currentYear = currentYear;
    yearlyData.dataLoaded = false;
    
    // Initialiser toutes les valeurs à NAN
    for (int day = 0; day < 366; day++) {
        for (int hour = 0; hour < 24; hour++) {
            yearlyData.temperaturesByDayHour[day][hour] = NAN;
        }
    }
    
    // Charger par chunks de 7 jours pour être plus efficace
    bool hasData = false;
    for (int startDay = 0; startDay < 366; startDay += 7) {
        int endDay = min(startDay + 6, 365);
        
        if (loadWeatherChunk(currentYear, startDay, endDay)) {
            hasData = true;
        }
        
        delay(1000); // Pause entre requêtes
        
        // Yield pour éviter le watchdog
        yield();
    }
    
    if (hasData) {
        yearlyData.dataLoaded = true;
        yearlyData.lastUpdate = millis();
        
        // Sauvegarder les nouvelles données
        saveYearlyDataToFlash();
        Serial.println("✅ Données météo annuelles téléchargées et sauvegardées");
    } else {
        Serial.println("❌ Échec du téléchargement des données météo");
    }
}

float getCurrentYearlySetpoint() {
    if (!yearlyData.dataLoaded) {
        return 23.0; // Valeur par défaut
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return 23.0;
    }
    
    int dayOfYear = timeinfo.tm_yday;
    int hour = timeinfo.tm_hour;
    
    if (dayOfYear < 366 && hour < 24) {
        float temp = yearlyData.temperaturesByDayHour[dayOfYear][hour];
        if (!isnan(temp)) {
            // Adapter la température pour terrarium (offset +2°C par exemple)
            return temp + 2.0;
        }
    }
    
    return 23.0; // Fallback
}

void controlHeater(float currentTemperature) {
    // ⭐ Vérifier le mode de sécurité avant tout contrôle
  if (safety.emergencyShutdown || safety.currentLevel >= SAFETY_CRITICAL) {
      output = 0;
      analogWrite(HEATER_PIN, 0);
      Serial.println("🚨 Chauffage bloqué par le système de sécurité");
      return;
  }
    
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      Serial.println("❌ Impossible d'obtenir l'heure pour la consigne");
      escalateSafety(SAFETY_WARNING, "Horloge non synchronisée");
      // Mode sécurisé: utiliser une température fixe
      setpoint = 23.0;
  } else {
      int hour = timeinfo.tm_hour;
      
      if (useTempCurve && hour >= 0 && hour < TEMP_CURVE_POINTS) {
          setpoint = tempCurve[hour];
          Serial.printf("🕐 %02d:%02d - Consigne: %.1f°C (courbe)\n", 
                        timeinfo.tm_hour, timeinfo.tm_min, setpoint);
      } else if (weatherModeEnabled) {
          setpoint = externalTemp; // Mode météo
          Serial.printf("🌤️ %02d:%02d - Consigne: %.1f°C (météo)\n", 
                        timeinfo.tm_hour, timeinfo.tm_min, setpoint);
      } else {
          // Mode manuel simple jour/nuit
          bool isDay = hour >= 8 && hour < 20;
          setpoint = isDay ? 25.0 : 22.0;
          Serial.printf("🌅 %02d:%02d - Consigne: %.1f°C (%s)\n", 
                        timeinfo.tm_hour, timeinfo.tm_min, setpoint, 
                        isDay ? "jour" : "nuit");
      }
  }

    // Appliquer les limitations de sécurité
  if (safety.currentLevel == SAFETY_WARNING) {
      output = min(output, 10.0); // Limiter en mode alerte
  }

  float currentMaxTemp = setpoint;
  float currentMinTemp = setpoint - hysteresis;
  unsigned long now = millis();

  if (currentTemperature >= currentMaxTemp) {
    output = 0;
    manualCycleOn = false;
  } 
  else if (currentTemperature < currentMinTemp) {
    output = 255;
    manualCycleOn = false;
  } 
  else {
    if (usePWM) {
      input = currentTemperature;
      myPID.Compute();
      output = constrain(output, 0, 255);
    } else {
      // Cycle manuel : 2s ON / 1s OFF
      if (manualCycleOn && now - lastToggleTime >= 990) {
        manualCycleOn = false;
        lastToggleTime = now;
      } else if (!manualCycleOn && now - lastToggleTime >= 2990) {
        manualCycleOn = true;
        lastToggleTime = now;
      }
      output = manualCycleOn ? 255 : 0;
    }
  }

  //Serial.printf("Heure: %d, Temp: %.1f, Cible: %.1f, Sortie: %d (%d%%), Mode: %s \n",
   //             hour, currentTemperature, setpoint, (int)output, (int)((output / 255.0) * 100),
   //             usePWM ? "PWM" : "Manuel");

  analogWrite(HEATER_PIN, output);
}

void updateInternalSensor() {
    safety.lastSensorRead = millis(); // ⭐ Marquer la tentative de lecture
    
    float temp = sht31.readTemperature();
    float hum = sht31.readHumidity();
    
    if (!isnan(temp) && !isnan(hum)) {
        // Vérifier si les valeurs sont cohérentes
        if (abs(temp - safety.lastKnownGoodTemp) > 15.0 && safety.lastKnownGoodTemp != 22.0) {
            Serial.printf("⚠️ Température suspecte: %.1f°C (dernière: %.1f°C)\n", 
                         temp, safety.lastKnownGoodTemp);
            // Ne pas mettre à jour immédiatement, attendre confirmation
            safety.temperatureOutOfRangeCount++;
            if (safety.temperatureOutOfRangeCount < 3) {
                return; // Ignorer cette lecture
            }
        }
        
        internalTemp = temp;
        internalHum = hum;
        safety.consecutiveFailures = 0; // Reset compteur d'échecs
        safety.temperatureOutOfRangeCount = 0;
        
        Serial.printf("🌡️ Capteurs OK: %.1f°C, %.0f%%\n", internalTemp, internalHum);
    } else {
        safety.consecutiveFailures++;
        Serial.printf("❌ Échec lecture capteur (tentative %d/%d)\n", 
                     safety.consecutiveFailures, MAX_CONSECUTIVE_FAILURES);
    }
    
    // ⭐ Vérifier les conditions de sécurité après chaque lecture
    checkSafetyConditions();
}

bool isHumidityValid(float newHumidity) {
    static float lastValidHumidity = 0.0;
    if (lastValidHumidity == 0.0) {
        lastValidHumidity = newHumidity;
        return true;
    }
    bool isValid = abs(newHumidity - lastValidHumidity) <= MAX_HUMIDITY_VARIATION;
    if (isValid) {
        lastValidHumidity = newHumidity;
    }
    return isValid;
}

bool isTemperatureValid(float newTemp) {
    if (lastTemperature == 0.0) return true;
    return abs(newTemp - lastTemperature) <= MAX_TEMP_VARIATION;
}

String readSht31Humidity() {
    float h = sht31.readHumidity();
    static float lastValidHumidity = 0.0;
    if (isnan(h)) {
        Serial.println("Échec de lecture d'humidité");
        if (lastValidHumidity != 0.0) {
            return String(lastValidHumidity, 1);
        }
        return "--";
    }
    if (!isHumidityValid(h)) {
        Serial.printf("Mesure d'humidité incohérente: %.1f%% (dernière: %.1f%%) \n", h, lastValidHumidity);
        h = lastValidHumidity;
    } else {
        lastValidHumidity = h;
        humiditeWindow[windowIndex] = h;
        windowIndex = (windowIndex + 1) % windowSize;

      if (windowIndex == 0) {
          windowFilled = true;
      }

      // Mise à jour des valeurs max et min sur les 1000 dernières mesures
      if (windowFilled) {
          maxHumidite = -INFINITY;
          minHumidite = INFINITY;
          for (int i = 0; i < windowSize; i++) {
              if (humiditeWindow[i] > maxHumidite) maxHumidite = humiditeWindow[i];
              if (humiditeWindow[i] < minHumidite) minHumidite = humiditeWindow[i];
          }
      } else {
          if (h > maxHumidite) maxHumidite = h;
          if (h < minHumidite) minHumidite = h;
      }
  }
    lastValidHumidity = h;
    return String(h, 1);
}

String readSht31Temperature() {
    float t = sht31.readTemperature();
    if (isnan(t)) {
        Serial.println("Échec de lecture de température");
        if (lastTemperature != 0.0) {
            controlHeater(lastTemperature);
            return String(lastTemperature);
        }
        return "--";
    }

    if (!isTemperatureValid(t)) {
      Serial.printf("Mesure incohérente: %.1f°C (dernière: %.1f°C) \n", t, lastTemperature);
      t = lastTemperature;
    } else {
        lastTemperature = t;
        temperatureWindow[windowIndex] = t;
        windowIndex = (windowIndex + 1) % windowSize;

      if (windowIndex == 0) {
          windowFilled = true;
      }

      // Mise à jour des valeurs max et min sur les 1000 dernières mesures
      if (windowFilled) {
          maxTemperature = -INFINITY;
          minTemperature = INFINITY;
          for (int i = 0; i < windowSize; i++) {
              if (temperatureWindow[i] > maxTemperature) maxTemperature = temperatureWindow[i];
              if (temperatureWindow[i] < minTemperature) minTemperature = temperatureWindow[i];
          }
      } else {
          if (t > maxTemperature) maxTemperature = t;
          if (t < minTemperature) minTemperature = t;
      }
  }
  controlHeater(t);
  addToHistory(t, sht31.readHumidity());
  return String(t, 1);
}

String getMovingAverageTemp() {
  float sum = 0;
  int count = windowFilled ? windowSize : windowIndex;

  if (count == 0) return "--";

  for (int i = 0; i < count; i++) {
      sum += temperatureWindow[i];
  }
  float average = sum / count;
  return String(average, 1);
}

String getMovingAverageHum() {
  float sum = 0;
  int count = windowFilled ? windowSize : windowIndex;

  if (count == 0) return "--";

  for (int i = 0; i < count; i++) {
      sum += humiditeWindow[i];
  }
  float average = sum / count;
  return String(average, 1);
}

String getHeaterState() {
    int percentage = map(output, 0, 255, 0, 100);
    return String(percentage);
}

String getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "--";
    }
    char timeString[9];
    strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
    return String(timeString);
}

void handlePWMModeSetting(AsyncWebServerRequest *request) {
  if (request->hasParam("usePWM")) {
    bool oldUsePWM = usePWM;
    usePWM = request->getParam("usePWM")->value() == "1";
    Preferences prefs;
    prefs.begin("settings", false);
    preferences.putBool("usePWM", usePWM);
    prefs.end();
    Serial.printf("[SET] Mode PWM sauvegardé : %s (avant : %s)\n", 
                  usePWM ? "activé" : "désactivé", oldUsePWM ? "activé" : "désactivé");
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètre manquant");
  }
}

void handleTemperatureSetting(AsyncWebServerRequest *request) {
    if (request->hasParam("hour") && request->hasParam("temp")) {
        int hour = request->getParam("hour")->value().toInt();
        float temp = request->getParam("temp")->value().toFloat();
        
        if (hour >= 0 && hour < TEMP_CURVE_POINTS) {
            tempCurve[hour] = temp;
            preferences.begin("settings", false);
            String key = "temp_" + String(hour);
            preferences.putFloat(key.c_str(), temp);
            preferences.end();
            
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Heure invalide");
        }
    } else {
        request->send(400, "text/plain", "Paramètres manquants");
    }
}

void handleGlobalMinTempSetting(AsyncWebServerRequest *request) {
  if (request->hasParam("globalMinTempSet")) {
    float oldValue = globalMinTempSet;
    globalMinTempSet = request->getParam("globalMinTempSet")->value().toFloat();

    if (globalMinTempSet < 0.1 || globalMinTempSet > 40.0) {
      request->send(400, "text/plain", "Valeur invalide");
      return;
    }
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putFloat("globalMinTempSet", globalMinTempSet);
    prefs.end(); // ✅ Fermer après

    Serial.printf("[SET] globalMinTempSet sauvegardée : %.1f (ancienne : %.1f)\n", globalMinTempSet, oldValue);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètre manquant");
  }
}

void handleGlobalMaxTempSetting(AsyncWebServerRequest *request) {
  if (request->hasParam("globalMaxTempSet")) {
    float oldValue = globalMaxTempSet;
    globalMaxTempSet = request->getParam("globalMaxTempSet")->value().toFloat();

    if (globalMaxTempSet < 0.1 || globalMaxTempSet > 40.0) {
      request->send(400, "text/plain", "Valeur invalide");
      return;
    }
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putFloat("globalMaxTempSet", globalMaxTempSet);
    prefs.end(); // ✅ Fermer après

    Serial.printf("[SET] globalMaxTempSet sauvegardée : %.1f (ancienne : %.1f)\n", globalMaxTempSet, oldValue);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètre manquant");
  }
}

void handleHysteresisSetting(AsyncWebServerRequest *request) {
  if (request->hasParam("hysteresis")) {
    float oldValue = hysteresis;
    hysteresis = request->getParam("hysteresis")->value().toFloat();

    if (hysteresis < 0.1 || hysteresis > 5.0) {
      request->send(400, "text/plain", "Valeur invalide");
      return;
    }
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putFloat("hysteresis", hysteresis);
    prefs.end(); // ✅ Fermer après

    Serial.printf("[SET] Hystérésis sauvegardée : %.1f (ancienne : %.1f)\n", hysteresis, oldValue);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètre manquant");
  }
}

void handlePIDSetting(AsyncWebServerRequest *request) {
  if (request->hasParam("Kp") && request->hasParam("Ki") && request->hasParam("Kd")) {
    float oldKp = Kp, oldKi = Ki, oldKd = Kd;

    Kp = request->getParam("Kp")->value().toFloat();
    Ki = request->getParam("Ki")->value().toFloat();
    Kd = request->getParam("Kd")->value().toFloat();

    myPID.SetTunings(Kp, Ki, Kd);
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putFloat("Kp", Kp);
    prefs.putFloat("Ki", Ki);
    prefs.putFloat("Kd", Kd);
    prefs.end();
    Serial.printf("[SET] PID sauvegardé : Kp=%.1f, Ki=%.1f, Kd=%.1f (avant : %.1f / %.1f / %.1f)\n", 
                  Kp, Ki, Kd, oldKp, oldKi, oldKd);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètres manquants");
  }
}

void renderOLEDPage(int page, int xOffset) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  //display.setCursor(xOffset, 0);
  display.setCursor(0, 0);
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int hour = timeinfo.tm_hour;
  bool isDay = hour >= 8 && hour < 20;
  bool heatingOn = (output > 0);

  switch (page) {
    case 0:
      //si mode météo weatherModeEnabled
      if (weatherModeEnabled) {
        display.println("Temp(c) Int / Ext");
        display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
        display.setCursor(0, 13);
        display.setTextSize(2);
        display.printf("%.1f %.1f\n", internalTemp, externalTemp);

        display.setCursor(0, 34);
        display.setTextSize(1);
        display.println("Hum Int / Ext");
        display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
        display.setCursor(0, 47);
        display.setTextSize(2);  
        display.printf("%.0f%%   %.0f%%", internalHum, externalHum);

      } else {
        display.println("Temp Int");
        display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
        display.setCursor(0, 13);
        display.setTextSize(2);
        display.printf("%.1fC\n", internalTemp);

        display.setCursor(0, 34);
        display.setTextSize(1);
        display.println("Hum Int");
        display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
        display.setCursor(0, 47);
        display.setTextSize(2);  
        display.printf("%.0f%%", internalHum);
      }
      break;

    case 1:
    //si différent de mode météo
    if (!weatherModeEnabled) {
      display.setTextSize(1);
      display.println("Consignes Jour/Nuit");
      display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
      display.setCursor(0, 13);
      display.setTextSize(2);
      display.setCursor(0, 47);
      display.setTextSize(1);
      display.printf(isDay ? "Jour " : "Nuit ");
      display.printf(heatingOn ? " On " : " Off ");
      display.printf(" T %.1fC\n", internalTemp);
    } else {
      display.setTextSize(1);
      display.println("Consignes meteo");
      display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
      display.setCursor(0, 13);
      display.setTextSize(2);
      display.printf("J %.1fC\n", externalTemp);
      display.setCursor(0, 47);
      display.setTextSize(1);
      display.printf(heatingOn ? "Chauf On " : "Chauf Off ");
      display.printf("%.1fC\n", internalTemp);
    }
      break;

    case 2:
      //si mode météo weatherModeEnabled
      if (weatherModeEnabled) {
        display.println("Temp Int / Ext");
        display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
        display.setCursor(0, 13);
        display.setTextSize(2);
        display.printf("%.1fC %.1fC\n", internalTemp, externalTemp);

        display.setCursor(0, 34);
        display.setTextSize(1);
        display.println("Hum Int / Ext");
        display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
        display.setCursor(0, 47);
        display.setTextSize(2);  
        display.printf("%.0f%%   %.0f%%", internalHum, externalHum);

      } else {
        display.println("Temp Int");
        display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
        display.setCursor(0, 13);
        display.setTextSize(2);
        display.printf("%.1fC\n", internalTemp);

        display.setCursor(0, 34);
        display.setTextSize(1);
        display.println("Hum Int");
        display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
        display.setCursor(0, 47);
        display.setTextSize(2);  
        display.printf("%.0f%%", internalHum);
      }
      break;

    case 3:
      display.setTextSize(1);
      display.println("Heure actuelle");
      display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
      display.setCursor(0, 13);
      display.setTextSize(2);
      char buf[10];
      strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
      display.println(buf);
      display.setTextSize(2);
      display.println(heatingOn ? "Chauf On" : "Chauf Off");
      display.printf("%.1fC\n", internalTemp);
      
      break;
  }

  display.display();
}

void slideToNextPage() {
  int from = 0;
  //for (int offset = 0; offset <= SCREEN_WIDTH; offset += 8) {
    display.clearDisplay();
    //renderOLEDPage(displayPage, -offset); // current slides left
    //renderOLEDPage((displayPage + 1) % pageCount, SCREEN_WIDTH - offset); // next comes in
    renderOLEDPage((displayPage + 1) % pageCount, SCREEN_WIDTH );
    //delay(15);
  //}
  displayPage = (displayPage + 1) % pageCount;
}

String processor(const String& var) {
    if (var == "USEPWM") return usePWM ? "true" : "false";
    if (var == "TEMPERATURE") return readSht31Temperature();
    if (var == "HUMIDITY") return readSht31Humidity();
    if (var == "HEATERSTATE") return getHeaterState();
    if (var == "HEATERCLASS") {
      int percentage = map(output, 0, 255, 0, 100);
      if (percentage > 60) return "heater-high";
      else if (percentage > 20) return "heater-medium";
      else return "heater-low";
    }
    if (var == "CURRENTTIME") return getCurrentTime();
    if (var == "MOVINGAVERAGETEMP") return getMovingAverageTemp();
    if (var == "MOVINGAVERAGEHUM") return getMovingAverageHum();
    if (var == "HYSTERESIS") return String(hysteresis, 1);
    if (var == "KP") return String(Kp, 1);
    if (var == "KI") return String(Ki, 1);
    if (var == "KD") return String(Kd, 1);

    if (var == "LATITUDE") return String(latitude, 4);
    if (var == "LONGITUDE") return String(longitude, 4);
    if (var == "WEATHERMODE") return weatherModeEnabled ? "true" : "false";
    
    return String();
}

// Ajouter cette fonction après la fonction setup()
void loadTempCurve() {

  // Charger les valeurs depuis les préférences
  useTempCurve = preferences.getBool("useTempCurve", false);
  
  for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
    String key = "temp_" + String(i);
    tempCurve[i] = preferences.getFloat(key.c_str(), tempCurve[i]);
  }
  
  Serial.printf("[LOAD] Courbe de température : %s\n", useTempCurve ? "activée" : "désactivée");
}

// Mode sécurisé en cas de panne capteur
void activateSafeMode() {
    Serial.println("🚨 MODE SÉCURISÉ ACTIVÉ");
    analogWrite(HEATER_PIN, 0); // Arrêter le chauffage
    // Afficher sur OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("MODE SECURITE");
    display.println("Capteur HS");
    display.display();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Démarrage du système de contrôle de température ===");

    //watchdog
    esp_task_wdt_init(30, true); // 30 secondes timeout
    esp_task_wdt_add(NULL);

    // === Initialisation des préférences === 
    if (!preferences.begin("system", false)) {
        Serial.println("❌ Échec d'initialisation des préférences ! ");
    } else {
        Serial.println("✅ Préférences initialisées");
        loadCompleteConfig(); // ⭐ Charger la config AVANT tout le reste
    }

  if (!LittleFS.begin(true)) {
    Serial.println("❌ LittleFS mount failed");
  } else {
    Serial.println("✅ LittleFS mount OK");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
      Serial.printf("✔️ Fichier LittleFS : ");
      Serial.println(file.name());
      file = root.openNextFile();
    }
  }
  
  // === Définition des routes HTTP ===
  Serial.println("LittleFS monté avec succès");
    // Servir le dossier "/css/" (fichiers CSS)
  server.serveStatic("/css/", LittleFS, "/css/")
    .setCacheControl("max-age=86400"); // (Optionnel) Cache pour améliorer les performances

  // Servir le dossier "/js/" (fichiers JS)
  server.serveStatic("/js/", LittleFS, "/js/");
  server.serveStatic("/images/", LittleFS, "/images/")
    .setDefaultFile("favico.ico");
  // Servir la page d'accueil
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
 
  // === Chargement des paramètres mémorisés ===
 bool cameraEnabled = preferences.getBool("camera", false);


  usePWM = preferences.getBool("usePWM", false);
  weatherModeEnabled = preferences.getBool("weatherMode", false);
  hysteresis = preferences.getFloat("hysteresis", 0.1);
  Kp = preferences.getFloat("Kp", 2.0);
  Ki = preferences.getFloat("Ki", 5.0);
  Kd = preferences.getFloat("Kd", 1.0);
  latitude = preferences.getFloat("latitude", 48.85);
  longitude = preferences.getFloat("longitude", 2.35);

  // === Initialisation WiFi ===
  Serial.print("Connexion au réseau WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connecté! IP: " + WiFi.localIP().toString());

  // === Configuration capteurs et périphériques ===
  Wire.begin(I2C_SDA, I2C_SCL);
  pinMode(HEATER_PIN, OUTPUT);
  analogWrite(HEATER_PIN, 10);
  delay(500);
  analogWrite(HEATER_PIN, 0);

  if (!sht31.begin(0x44)) {
    Serial.println("❌ Capteur SHT30 non trouvé !");
    while (true);
  }

  // === Initialisation de l'écran OLED ===
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("❌ Échec d'initialisation OLED");
    while (true);
  }
  display.clearDisplay();
  display.setCursor(0, 0);
  display.drawBitmap(positionImageAxeHorizontal, positionImageAxeVertical, logo, largeurDeLimage, hauteurDeLimage, WHITE);
  display.display();
  
  
    // Configuration de la caméra
  camera_config_t cam_config;
  cam_config.ledc_channel = LEDC_CHANNEL_0;
  cam_config.ledc_timer = LEDC_TIMER_0;
  cam_config.pin_d0 = Y2_GPIO_NUM;
  cam_config.pin_d1 = Y3_GPIO_NUM;
  cam_config.pin_d2 = Y4_GPIO_NUM;
  cam_config.pin_d3 = Y5_GPIO_NUM;
  cam_config.pin_d4 = Y6_GPIO_NUM;
  cam_config.pin_d5 = Y7_GPIO_NUM;
  cam_config.pin_d6 = Y8_GPIO_NUM;
  cam_config.pin_d7 = Y9_GPIO_NUM;
  cam_config.pin_xclk = XCLK_GPIO_NUM;
  cam_config.pin_pclk = PCLK_GPIO_NUM;
  cam_config.pin_vsync = VSYNC_GPIO_NUM;
  cam_config.pin_href = HREF_GPIO_NUM;
  cam_config.pin_sccb_sda = SIOD_GPIO_NUM;
  cam_config.pin_sccb_scl = SIOC_GPIO_NUM;
  cam_config.pin_pwdn = PWDN_GPIO_NUM;
  cam_config.pin_reset = RESET_GPIO_NUM;
  cam_config.xclk_freq_hz = 20000000;
  cam_config.pixel_format = PIXFORMAT_JPEG;
  
  // Initialisation avec les paramètres d'image
  //config.frame_size = FRAMESIZE_QXGA;  // ou FRAMESIZE_VGA
  cam_config.frame_size = FRAMESIZE_VGA;
	cam_config.jpeg_quality = 10;               // ✅ qualité correcte
	cam_config.fb_count = 1;  
  
  // Initialisation de la caméra
  esp_err_t err = esp_camera_init(&cam_config);
  if (err != ESP_OK) {
    Serial.printf("Échec d'initialisation de la caméra: 0x%x \n", err);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Erreur caméra");
    display.display();
    return;
  }
  
  Serial.println("Caméra et écran initialisés avec succès");
  
	display.clearDisplay();
	display.setCursor(0, 0);
	display.println("Wifi");
	display.println(WiFi.localIP());
	display.display();
	
  // === Initialisation PID ===
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 255);
  myPID.SetTunings(Kp, Ki, Kd);

  // === Initialisation horloge (NTP) ===
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    ExternalWeather ext = getWeatherData();
    if (!isnan(ext.temperature)) externalTemp = ext.temperature;
    if (!isnan(ext.humidity)) externalHum = ext.humidity;
    Serial.println(&timeinfo, "Date : %A, %d %B %Y - %H:%M:%S");
  } else {
    Serial.println("❌ Impossible d'obtenir l'heure");
  }
	// curv tempCurve
	loadTempCurve();
	
server.on("/safetyStatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    
    doc["level"] = safety.currentLevel;
    doc["levelName"] = (safety.currentLevel == SAFETY_NORMAL) ? "Normal" :
                      (safety.currentLevel == SAFETY_WARNING) ? "Alerte" :
                      (safety.currentLevel == SAFETY_CRITICAL) ? "Critique" : "Urgence";
    doc["emergencyShutdown"] = safety.emergencyShutdown;
    doc["lastError"] = safety.lastErrorMessage;
    doc["consecutiveFailures"] = safety.consecutiveFailures;
    doc["lastSensorRead"] = (millis() - safety.lastSensorRead) / 1000;
    doc["lastKnownTemp"] = safety.lastKnownGoodTemp;
    doc["lastKnownHum"] = safety.lastKnownGoodHum;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
});

server.on("/resetSafety", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (safety.currentLevel <= SAFETY_WARNING) {
        safety.currentLevel = SAFETY_NORMAL;
        exitSafeMode();
        request->send(200, "text/plain", "Système de sécurité réinitialisé");
    } else {
        request->send(403, "text/plain", "Impossible de réinitialiser en mode critique/urgence");
    }
});

server.on("/weatherFiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<h2>Fichiers Météo Stockés</h2><ul>";
    
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    
    while (file) {
        String fileName = file.name();
        if (fileName.startsWith("weather_") && fileName.endsWith(".json")) {
            size_t fileSize = file.size();
            html += "<li>" + fileName + " (" + String(fileSize) + " octets)</li>";
        }
        file = root.openNextFile();
    }
    
    html += "</ul>";
    html += "<p>Espace libre: " + String(LittleFS.totalBytes() - LittleFS.usedBytes()) + " octets</p>";
    
    request->send(200, "text/html", html);
});

server.on("/clearWeatherCache", HTTP_GET, [](AsyncWebServerRequest *request) {
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    int deletedCount = 0;
    
    while (file) {
        String fileName = file.name();
        if (fileName.startsWith("weather_") && fileName.endsWith(".json")) {
            LittleFS.remove("/" + fileName);
            deletedCount++;
        }
        file = root.openNextFile();
    }
    
    request->send(200, "text/plain", "Cache météo vidé: " + String(deletedCount) + " fichiers supprimés");
});

	server.on("/getTempCurve", HTTP_GET, [](AsyncWebServerRequest *request) {
	  DynamicJsonDocument doc(1024);
	  JsonArray array = doc.to<JsonArray>();
	  
	  for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
		array.add(tempCurve[i]);
	  }
	  
	  String response;
	  serializeJson(doc, response);
	  request->send(200, "application/json", response);
	});

	server.on("/setTempCurveMode", HTTP_GET, [](AsyncWebServerRequest *request) {
	  if (request->hasParam("enabled")) {
		bool oldValue = useTempCurve;
		useTempCurve = request->getParam("enabled")->value() == "1";
		
		Preferences prefs;
		prefs.begin("settings", false);
		prefs.putBool("useTempCurve", useTempCurve);
		prefs.end();
		
		Serial.printf("[SET] Mode courbe de température : %s (avant : %s)\n", 
					  useTempCurve ? "activé" : "désactivé", 
					  oldValue ? "activé" : "désactivé");
		request->send(200, "text/plain", "OK");
	  } else {
		request->send(400, "text/plain", "Paramètre manquant");
	  }
	});
/*
	server.on("/setTempCurve", HTTP_POST, 
	  [](AsyncWebServerRequest *request) {
		// On ne fait rien ici, tout est géré dans le handler suivant
		request->send(400, "text/plain", "Erreur: données JSON attendues");
	  },
	  NULL,
	  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		DynamicJsonDocument doc(2048);
		DeserializationError error = deserializeJson(doc, data, len);
		
		if (error) {
		  request->send(400, "text/plain", "Erreur de parsing JSON");
		  return;
		}
		
		if (!doc.is<JsonArray>()) {
		  request->send(400, "text/plain", "Format attendu: tableau de valeurs");
		  return;
		}
		
		JsonArray arr = doc.as<JsonArray>();
		if (arr.size() != TEMP_CURVE_POINTS) {
		  request->send(400, "text/plain", "Nombre de points incorrect");
		  return;
		}
		
		Preferences prefs;
		prefs.begin("settings", false);
		
		for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
		  float temp = arr[i].as<float>();
		  
		  // Validation des valeurs
		  if (temp < 5.0 || temp > 35.0) {
			request->send(400, "text/plain", "Valeurs hors limites (5-35°C)");
			prefs.end();
			return;
		  }
		  
		  tempCurve[i] = temp;
		  String key = "temp_" + String(i);
		  prefs.putFloat(key.c_str(), temp);
		}
		
		prefs.end();
		Serial.println("[SET] Courbe de température mise à jour");
		request->send(200, "text/plain", "OK");
	  }
	);
*/

server.on("/setTempCurve", HTTP_POST, 
    [](AsyncWebServerRequest *request) {}, // Gestionnaire vide
    NULL, // Téléchargement de fichier non géré
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Vérifier le type de contenu
        if (strstr(request->contentType().c_str(), "application/json") == NULL) {
            request->send(400, "text/plain", "Content-Type must be application/json");
            return;
        }

        // Parser le JSON
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (error) {
            request->send(400, "text/plain", "Erreur de parsing JSON");
            return;
        }

        // Vérifier que c'est un tableau
        if (!doc.is<JsonArray>()) {
            request->send(400, "text/plain", "Format attendu: tableau de températures");
            return;
        }

        // Mettre à jour les températures
        JsonArray arr = doc.as<JsonArray>();
        for (size_t i = 0; i < arr.size() && i < TEMP_CURVE_POINTS; i++) {
            tempCurve[i] = arr[i].as<float>();
        }

        request->send(200, "text/plain", "OK");
    }
);

server.on("/updateConfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Vérifier le type de contenu
    if (request->contentType() != "application/json") {
        request->send(400, "text/plain", "Content-Type must be application/json");
        return;
    }

    // Traiter le corps de la requête
    String body = request->arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        request->send(400, "text/plain", "Invalid JSON");
        return;
    }

    // Mettre à jour la configuration
    config.hysteresis = doc["hysteresis"] | config.hysteresis;
    config.Kp = doc["Kp"] | config.Kp;
    config.Ki = doc["Ki"] | config.Ki;
    config.Kd = doc["Kd"] | config.Kd;
    config.usePWM = doc["usePWM"] | config.usePWM;
    config.latitude = doc["latitude"] | config.latitude;
    config.longitude = doc["longitude"] | config.longitude;
    config.weatherModeEnabled = doc["weatherMode"] | config.weatherModeEnabled;
    config.cameraEnabled = doc["cameraEnabled"] | config.cameraEnabled;
    config.globalMinTempSet = doc["globalMinTempSet"] | config.globalMinTempSet;
    config.globalMaxTempSet = doc["globalMaxTempSet"] | config.globalMaxTempSet;

    // Sauvegarder
    saveCompleteConfig();

    request->send(200, "text/plain", "Configuration sauvegardée");
});

  server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    doc["hysteresis"] = hysteresis;
    doc["Kp"] = Kp;
    doc["Ki"] = Ki;
    doc["Kd"] = Kd;
    doc["weatherMode"] = weatherModeEnabled;
    doc["usePWM"] = usePWM;
    doc["latitude"] = latitude;
    doc["longitude"] = longitude;
    doc["cameraEnabled"] = preferences.getBool("camera", false);
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/setTemp", HTTP_GET, handleTemperatureSetting);
  server.on("/setHysteresis", HTTP_GET, handleHysteresisSetting);
  server.on("/minTempSet", HTTP_GET, [](AsyncWebServerRequest *request) {
  request->send(200, "text/plain", String(globalMinTempSet, 1));
});
server.on("/maxTempSet", HTTP_GET, [](AsyncWebServerRequest *request) {
  request->send(200, "text/plain", String(globalMaxTempSet, 1));
});

  server.on("/setGlobalMinTemp", HTTP_GET, handleGlobalMinTempSetting);
  server.on("/setGlobalMaxTemp", HTTP_GET, handleGlobalMaxTempSetting);
  server.on("/setPID", HTTP_GET, handlePIDSetting);
  server.on("/setPWMMode", HTTP_GET, handlePWMModeSetting);

  server.on("/setCamera", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("enabled")) {
    bool enabled = request->getParam("enabled")->value().toInt() == 1;

    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putBool("camera", enabled);
    prefs.end();

    Serial.printf("[SET] Caméra: %s\n", enabled ? "activée" : "désactivée");
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètre manquant");
  }
});

  server.on("/setWeather", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("lat") && request->hasParam("lon") && request->hasParam("enabled")) {
    latitude = request->getParam("lat")->value().toFloat();
    longitude = request->getParam("lon")->value().toFloat();
    weatherModeEnabled = request->getParam("enabled")->value().toInt() == 1;

    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putFloat("latitude", latitude);
    prefs.putFloat("longitude", longitude);
    prefs.putBool("weatherMode", weatherModeEnabled);  // ✅ clé raccourcie
    bool confirm = prefs.getBool("weatherMode", false);
    prefs.end();

    Serial.printf("[SET] Météo sauvegardée: enabled=%s, lat=%.4f, lon=%.4f\n",
                  weatherModeEnabled ? "true" : "false", latitude, longitude);
    Serial.printf("[DEBUG] Relecture après écriture: weatherMode = %s\n",
                  confirm ? "true" : "false");

    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètres manquants");
  }
});

  server.on("/weatherData", HTTP_GET, [](AsyncWebServerRequest *request) {
    ExternalWeather w = getWeatherData();
    if (isnan(w.temperature) || isnan(w.humidity)) {
      request->send(503, "application/json", "{\"error\":\"invalid data\"}");
    } else {
      String json = "{\"temp\":" + String(w.temperature, 1) + ",\"hum\":" + String(w.humidity, 0) + "}";
      request->send(200, "application/json", json);
    }
  });

  // === Autres endpoints ===
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", readSht31Temperature().c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", readSht31Humidity().c_str());
  });
  server.on("/heaterState", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getHeaterState().c_str());
  });
  server.on("/currentTime", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getCurrentTime().c_str());
  });
  server.on("/movingAverageTemp", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getMovingAverageTemp().c_str());
  });
   server.on("/movingAverageHum", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getMovingAverageHum().c_str());
  });
  server.on("/maxTemperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(maxTemperature, 1));
  });
  server.on("/minTemperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(minTemperature, 1));
  });
    server.on("/maxHumidite", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(maxHumidite, 1));
  });
  server.on("/minHumidite", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(minHumidite, 1));
  });
  server.on("/history", HTTP_GET, handleHistoryRequest);

  // === Favicon & gestion 404 ===
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    request->send(500, "text/plain", "Erreur caméra");
    return;
  }

  AsyncWebServerResponse *response = request->beginResponse(
    "image/jpeg", fb->len,
    [fb](uint8_t *buffer, size_t maxLen, size_t alreadySent) -> size_t {
      if (alreadySent >= fb->len) return 0;
      size_t toCopy = min(fb->len - alreadySent, maxLen);
      memcpy(buffer, fb->buf + alreadySent, toCopy);
      if (alreadySent + toCopy >= fb->len) esp_camera_fb_return(fb);
      return toCopy;
    });

  response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
  request->send(response);
});

  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.printf("❌ Route non trouvée : %s\n", request->url().c_str());
    request->send(404, "text/plain", "Not found");
  });

// === Logs des préférences ===
  server.begin();
  Serial.println("✅ Serveur HTTP démarré");
  Serial.println("===================================");
  Serial.println("[BOOT] Chargement des préférences");
    if (!psramFound()) {Serial.println("❌ PSRAM non détectée !");}
  Serial.println("===================================");
  Serial.printf("→ Paramètres chargés:\n");
  Serial.printf("  usePWM: %s\n", usePWM ? "true" : "false");
  Serial.printf("  weatherModeEnabled: %s\n", weatherModeEnabled ? "true" : "false");
  Serial.printf("  cameraEnabled: %s\n", cameraEnabled ? "true" : "false");
  Serial.printf("  PID: Kp=%.1f, Ki=%.1f, Kd=%.1f\n", Kp, Ki, Kd);
  Serial.printf("  Hystérésis: %.1f \n", hysteresis);
  Serial.printf("  Coord GPS: %.4f, %.4f\n", latitude, longitude);
  Serial.println("===================================");
}

void loop() {
  // reset watchdog
  esp_task_wdt_reset(); 

    static unsigned long lastUpdate = 0;
    static unsigned long lastSafetyCheck = 0;
    
    // ⭐ Vérification de sécurité toutes les 5 secondes
    if (millis() - lastSafetyCheck > 5000) {
        checkSafetyConditions();
        lastSafetyCheck = millis();
    }
    
    // Mise à jour normale toutes les 10 secondes
    if (millis() - lastUpdate > 10000) {
        updateInternalSensor(); // Inclut déjà checkSafetyConditions()
        slideToNextPage();
        lastUpdate = millis();
    }
    
    // Gestion de l'affichage d'urgence (clignotement)
    if (safety.currentLevel == SAFETY_EMERGENCY) {
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 1000) { // Clignoter chaque seconde
            activateEmergencyMode(safety.lastErrorMessage);
            lastBlink = millis();
        }
    }

    if (millis() - lastUpdate > 10000) {
      updateInternalSensor();
      slideToNextPage();
      lastUpdate = millis();
    }
    if (weatherModeEnabled && millis() - lastWeatherUpdate > weatherInterval) {
      lastWeatherUpdate = millis();
      ExternalWeather ext = getWeatherData();
      if (!isnan(ext.temperature)) {
        externalTemp = ext.temperature;
        setpoint = externalTemp;
        Serial.printf("Setpoint météo mis à %.1f°C \n", setpoint);
      }
      if (!isnan(ext.humidity)) {
        externalHum = ext.humidity;
      }
    }

  delay(500);
}
