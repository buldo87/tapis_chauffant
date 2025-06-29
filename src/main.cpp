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
#include <Adafruit_NeoPixel.h>
#include "esp_camera.h"
#include "logo.h"
#include "LittleFS.h"
#include "esp_task_wdt.h"
#include "esp_mac.h"

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

// Configuration NeoPixel
#define PIN 48
#define NUMPIXELS 1

// Définition des broches I2C pour l'écran SSD1306
#define I2C_SDA 1
#define I2C_SCL 2

// Configuration de l'écran SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// === Tapis Chauffant ===
#define HEATER_PIN 42
// === Courbe de température ===
const int TEMP_CURVE_POINTS = 24;  // Un point par heure


// === STRUCTURE DE CONFIGURATION UNIFIÉE ===
struct SystemConfig {
    // Paramètres de contrôle
    bool usePWM = false;
    bool weatherModeEnabled = false;
    bool cameraEnabled = false;
	String cameraResolution = "qvga";
    bool useTempCurve = false;
    bool useLimitTemp = true;
    
    // Paramètres PID et contrôle
    float hysteresis = 0.3;
    float Kp = 2.0, Ki = 5.0, Kd = 1.0;
    double setpoint = 23.0;
    
    // Géolocalisation
    float latitude = 48.85;
    float longitude = 2.35;
    int DST_offset = 2;
    
    // Limites de température
    float globalMinTempSet = 15.0;
    float globalMaxTempSet = 35.0;
    
    // Courbe de température
    float tempCurve[TEMP_CURVE_POINTS];
    
    // LED
    bool ledState = false;
    int ledBrightness = 255;
    int ledRed = 255;
    int ledGreen = 255;
    int ledBlue = 255;
    
    // Métadonnées
    uint32_t configVersion = 1;
    uint32_t configHash = 0;
    char lastSaveTime[20] = "never";
    
    // Constructeur pour initialiser tempCurve
    SystemConfig() {
        for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
            tempCurve[i] = 22.2 + (i >= 8 && i <= 20 ? 3.0 : 0.0);
        }
    }
};
SystemConfig config;
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

// === Variables globales ===
// Capteurs et état système
float internalTemp = NAN;
float internalHum = NAN;
float externalTemp = 0.0;
float externalHum = 0.0;
bool heaterState = false;
float lastTemperature = 0.0;
double input, output;
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;
const float MAX_TEMP_VARIATION = 5.0;
const float MAX_HUMIDITY_VARIATION = 10.0;

// Fenêtres glissantes pour moyennes
const int windowSize = 1000;
float temperatureWindow[windowSize];
float humiditeWindow[windowSize];
int windowIndex = 0;
bool windowFilled = false;

// Statistiques
float maxTemperature = -INFINITY;
float minTemperature = INFINITY;
float maxHumidite = -INFINITY;
float minHumidite = INFINITY;

// Temporisation et cycles
unsigned long lastToggleTime = 0;
bool manualCycleOn = false;

// Sauvegarde optimisée
unsigned long lastSaveRequest = 0;
const unsigned long saveDelay = 5000;
bool savePending = false;

// === Historique circulaire ===
const int MAX_HISTORY_RECORDS = 1440;
// Historique 
HistoryRecord history[MAX_HISTORY_RECORDS];
int historyIndex = 0;
bool historyFull = false;

// Affichage OLED
unsigned long lastDisplayChange = 0;
int displayPage = 0;
const int pageCount = 4;

// Météo
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 3600000;

// Objets matériels (inchangés)
AsyncWebServer server(80);
Preferences preferences;
PID myPID(&input, &output, &config.setpoint, config.Kp, config.Ki, config.Kd, DIRECT);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

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

// logo
int16_t positionImageAxeHorizontal = 20;     // Position de la gauche de l’image à 20 pixels du bord gauche de l’écran
int16_t positionImageAxeVertical = 0;       // Position du haut de l’image à 16 pixels du bord haut de l’écran OLED
int16_t largeurDeLimage = 64;                // Largeur de l’image à afficher : 64 pixels
int16_t hauteurDeLimage = 64;                // Hauteur de l’image à afficher : 64 pixels

// === OPTIMISATION SAUVEGARDE ===
// === FONCTIONS DE CONFIGURATION SIMPLIFIÉES ===
uint32_t calculateConfigHash() {
    uint32_t hash = 0;
    uint8_t* data = (uint8_t*)&config;
    size_t dataSize = sizeof(config) - sizeof(config.lastSaveTime) - sizeof(config.configHash);
    
    for (size_t i = 0; i < dataSize; i++) {
        hash = hash * 31 + data[i];
    }
    return hash;
}

void saveConfigIfChanged() {
    uint32_t currentHash = calculateConfigHash();
    
    if (currentHash == config.configHash) {
        Serial.println("🟡 Aucune modification, sauvegarde ignorée.");
        return;
    }
    
    preferences.begin("system", false);
    size_t written = preferences.putBytes("config", &config, sizeof(config));
    
    if (written == sizeof(config)) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            strftime(config.lastSaveTime, sizeof(config.lastSaveTime), "%d-%m-%Y %H:%M:%S", &timeinfo);
            preferences.putString("lastSave", config.lastSaveTime);
        }
        
        config.configHash = currentHash;
        Serial.printf("💾 Config sauvegardée (%zu octets) à %s\n", written, config.lastSaveTime);
    } else {
        Serial.println("❌ Échec de la sauvegarde");
    }
    
    preferences.end();
}

void requestConfigSave() {
    lastSaveRequest = millis();
    savePending = true;
}

void loadCompleteConfig() {
    preferences.begin("system", true);
    size_t configSize = preferences.getBytesLength("config");
    
    if (configSize == sizeof(config)) {
        preferences.getBytes("config", &config, sizeof(config));
        Serial.println("✅ Configuration restaurée depuis la mémoire");
    } else {
        Serial.println("⚠️ Aucune configuration trouvée, utilisation des valeurs par défaut");
        // Le constructeur SystemConfig() a déjà initialisé tempCurve
    }
    
    String lastSave = preferences.getString("lastSave", "jamais");
    preferences.end();
    
    // Mise à jour du PID avec les valeurs chargées
    myPID.SetTunings(config.Kp, config.Ki, config.Kd);
    
    Serial.printf("📅 Dernière sauvegarde: %s\n", lastSave.c_str());
}

// === FONCTIONS DE SÉCURITÉ  ===
void activateWarningMode(const String& reason) {
    Serial.println("⚠️ MODE ALERTE ACTIVÉ: " + reason);
    
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
    
    if (output > 128) {
        output = 128;
        analogWrite(HEATER_PIN, output);
    }
}

void activateCriticalMode(const String& reason) {
    Serial.println("🔴 MODE CRITIQUE ACTIVÉ: " + reason);
    
    output = 0;
    analogWrite(HEATER_PIN, 0);
    
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
    
    internalTemp = safety.lastKnownGoodTemp;
    internalHum = safety.lastKnownGoodHum;
}

void activateEmergencyMode(const String& reason) {
    Serial.println("🚨 MODE URGENCE ACTIVÉ: " + reason);
    
    safety.emergencyShutdown = true;
    output = 0;
    analogWrite(HEATER_PIN, 0);
    
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
    delay(2000);
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
    
    if (now - safety.lastSensorRead > SENSOR_TIMEOUT) {
        escalateSafety(SAFETY_CRITICAL, "Capteurs non réactifs depuis " + String((now - safety.lastSensorRead)/1000) + "s");
        return;
    }
    
    if (!isnan(internalTemp)) {
        safety.lastValidTemperature = now;
        safety.lastKnownGoodTemp = internalTemp;
        
        if (internalTemp >= TEMP_EMERGENCY_HIGH || internalTemp <= TEMP_EMERGENCY_LOW) {
            escalateSafety(SAFETY_EMERGENCY, "Température critique: " + String(internalTemp) + "°C");
            return;
        } else if (internalTemp >= TEMP_WARNING_HIGH || internalTemp <= TEMP_WARNING_LOW) {
            escalateSafety(SAFETY_WARNING, "Température d'alerte: " + String(internalTemp) + "°C");
        } else {
            safety.temperatureOutOfRangeCount = 0;
        }
    } else {
        safety.consecutiveFailures++;
        if (safety.consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
            escalateSafety(SAFETY_CRITICAL, "Échecs consécutifs de lecture température: " + String(safety.consecutiveFailures));
            return;
        }
    }
    
    if (!isnan(internalHum)) {
        safety.lastValidHumidity = now;
        safety.lastKnownGoodHum = internalHum;
        
        if (internalHum >= HUM_EMERGENCY_HIGH || internalHum <= HUM_EMERGENCY_LOW) {
            escalateSafety(SAFETY_WARNING, "Humidité critique: " + String(internalHum) + "%");
        }
    }
    
    if (abs(internalTemp - safety.lastKnownGoodTemp) > 10.0) {
        safety.temperatureOutOfRangeCount++;
        if (safety.temperatureOutOfRangeCount >= 3) {
            escalateSafety(SAFETY_WARNING, "Variation température suspecte: " + String(internalTemp) + "°C vs " + String(safety.lastKnownGoodTemp) + "°C");
        }
    }
    
    if (output > 200 && internalTemp > config.setpoint + 2.0) {
        escalateSafety(SAFETY_WARNING, "Chauffage actif mais température élevée");
    }
    
    if (safety.currentLevel > SAFETY_NORMAL && now - safety.safetyActivatedTime > SAFETY_RESET_DELAY) {
        if (internalTemp > TEMP_WARNING_LOW && internalTemp < TEMP_WARNING_HIGH && 
            internalHum > HUM_EMERGENCY_LOW && internalHum < HUM_EMERGENCY_HIGH) {
            downgradeSafety();
        }
    }
}

// === FONCTIONS MÉTÉO Local ===
ExternalWeather getWeatherData() {
    ExternalWeather result = {NAN, NAN};
    
    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(config.latitude, 4) + 
                "&longitude=" + String(config.longitude, 4) + 
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
    
    for (size_t i = 0; i < timeArr.size(); i++) {
        if (timeArr[i].as<String>() == targetHour) {
            result.humidity = humArr[i].as<float>();
            break;
        }
    }
    
    return result;
}

// === FONCTIONS HISTORIQUE ===
void addToHistory(float temperature, float humidity) {
    time_t now = time(nullptr);
    history[historyIndex] = { now, temperature, humidity };
    historyIndex = (historyIndex + 1) % MAX_HISTORY_RECORDS;
    if (historyIndex == 0) historyFull = true;
}

// === CONTRÔLE CHAUFFAGE ===
void controlHeater(float currentTemperature) {
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
        config.setpoint = 23.0;
    } else {
        int hour = timeinfo.tm_hour;
        if (hour >= 0 && hour < TEMP_CURVE_POINTS) {
            config.setpoint = config.tempCurve[hour];
        }
    }

    if (safety.currentLevel == SAFETY_WARNING) {
        output = min(output, 128.0);
    }

    float currentMaxTemp = config.setpoint;
    float currentMinTemp = config.setpoint - config.hysteresis;
    unsigned long now = millis();

    if (currentTemperature >= currentMaxTemp) {
        output = 0;
        manualCycleOn = false;
    } else if (currentTemperature < currentMinTemp) {
        output = 255;
        manualCycleOn = false;
    } else {
        if (config.usePWM) {
            input = currentTemperature;
            myPID.Compute();
            output = constrain(output, 0, 255);
        } else {
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

    analogWrite(HEATER_PIN, output);
}

// === CAPTEURS ===
void updateInternalSensor() {
    safety.lastSensorRead = millis();
    
    float temp = sht31.readTemperature();
    float hum = sht31.readHumidity();
    
    if (!isnan(temp) && !isnan(hum)) {
        if (abs(temp - safety.lastKnownGoodTemp) > 15.0 && safety.lastKnownGoodTemp != 22.0) {
            Serial.printf("⚠️ Température suspecte: %.1f°C (dernière: %.1f°C)\n", temp, safety.lastKnownGoodTemp);
            safety.temperatureOutOfRangeCount++;
            if (safety.temperatureOutOfRangeCount < 3) {
                return;
            }
        }
        
        internalTemp = temp;
        internalHum = hum;
        safety.consecutiveFailures = 0;
        safety.temperatureOutOfRangeCount = 0;
        
       // Serial.printf("🌡️ Capteurs OK: %.1f°C, %.0f%%\n", internalTemp, internalHum);
    } else {
        safety.consecutiveFailures++;
        Serial.printf("❌ Échec lecture capteur (tentative %d/%d)\n", safety.consecutiveFailures, MAX_CONSECUTIVE_FAILURES);
    }
    
    checkSafetyConditions();
}

// === FONCTIONS UTILITAIRES ===
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
    
    lastTemperature = t;
    temperatureWindow[windowIndex] = t;
    windowIndex = (windowIndex + 1) % windowSize;
    
    if (windowIndex == 0) {
        windowFilled = true;
    }
    
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
    
    controlHeater(t);
    addToHistory(t, sht31.readHumidity());
    return String(t, 1);
}

String readSht31Humidity() {
    float h = sht31.readHumidity();
    if (isnan(h)) {
        Serial.println("Échec de lecture d'humidité");
        return "--";
    }
    
    humiditeWindow[windowIndex] = h;
    
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
    
    return String(h, 1);
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
        return "--:--";
    }
    
    char timeString[9];
    strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
    return String(timeString);
}

// === HANDLERS ===
void handleHistoryRequest(AsyncWebServerRequest *request) {
    String json = "[";
    int total = historyFull ? MAX_HISTORY_RECORDS : historyIndex;
    bool first = true;
    
    for (int i = 0; i < total; i++) {
        int idx = (historyIndex + i) % MAX_HISTORY_RECORDS;
        HistoryRecord r = history[idx];
        
        if (r.timestamp == 0) continue;
        
        if (!first) json += ",";
        first = false;
        
        json += String("{\"t\":") + r.timestamp + 
                ",\"temp\":" + String(r.temperature, 1) + 
                ",\"hum\":" + String(r.humidity, 1) + "}";
    }
    
    json += "]";
    request->send(200, "application/json", json);
}

void handleSaveConfiguration(AsyncWebServerRequest *request) {
    if (request->hasParam("applyBtn")) {
      //syncGlobalsToConfig();
    Serial.println("💾 Sauvegarde demandée");
    savePending = false;
    saveConfigIfChanged();

      request->send(200, "text/plain", "Configuration sauvegardée demandée handleSaveConfiguration");
    } else {
        request->send(400, "text/plain", "Paramètre manquant handleSaveConfiguration");
    }
}

void handleGlobalTempSettings(AsyncWebServerRequest *request) {
    if (request->url().endsWith("MinTemp")) {
        if (request->hasParam("globalMinTempSet")) {
            config.globalMinTempSet = request->getParam("globalMinTempSet")->value().toFloat();
            request->send(200, "text/plain", "Min OK");
        } else {
            request->send(200, "text/plain", "Valeur actuelle : " + String(config.globalMinTempSet));
        }
    } else if (request->url().endsWith("MaxTemp")) {
        if (request->hasParam("globalMaxTempSet")) {
            config.globalMaxTempSet = request->getParam("globalMaxTempSet")->value().toFloat();
            request->send(200, "text/plain", "Max OK");
        } else {
            request->send(200, "text/plain", "Valeur actuelle : " + String(config.globalMaxTempSet));
        }
    } else {
        request->send(404, "text/plain", "Route inconnue");
    }
}

void handlePWMModeSetting(AsyncWebServerRequest *request) {
    if (request->hasParam("usePWM")) {
        config.usePWM = request->getParam("usePWM")->value().toInt() == 1;
        Serial.printf("[CONF] Mode PWM : %s\n", config.usePWM ? "Activé" : "Désactivé");
        requestConfigSave();
        request->send(200, "text/plain", "PWM OK");
    } else {
        request->send(400, "text/plain", "Paramètre manquant");
    }
}

void handleHysteresisSetting(AsyncWebServerRequest *request) {
    if (request->hasParam("hysteresis")) {
        config.hysteresis = request->getParam("hysteresis")->value().toFloat();
        Serial.printf("[CONF] Hystérésis : %.2f\n", config.hysteresis);
        requestConfigSave();
        request->send(200, "text/plain", "Hystérésis OK");
    } else {
        request->send(400, "text/plain", "Paramètre manquant");
    }
}

void handlePIDSetting(AsyncWebServerRequest *request) {
    if (request->hasParam("Kp") && request->hasParam("Ki") && request->hasParam("Kd")) {
        config.Kp = request->getParam("Kp")->value().toFloat();
        config.Ki = request->getParam("Ki")->value().toFloat();
        config.Kd = request->getParam("Kd")->value().toFloat();

        myPID.SetTunings(config.Kp, config.Ki, config.Kd);

        Serial.printf("[CONF] PID Kp=%.2f, Ki=%.2f, Kd=%.2f\n", config.Kp, config.Ki, config.Kd);
        request->send(200, "text/plain", "PID OK");
    } else {
        request->send(400, "text/plain", "Paramètres manquants");
    }
}

void handleApplyAllSettings(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "text/plain", "Erreur de parsing JSON");
        return;
    }

    // Mise à jour directe de la structure config
    if (doc.containsKey("hysteresis")) config.hysteresis = doc["hysteresis"];
    if (doc.containsKey("Kp")) config.Kp = doc["Kp"];
    if (doc.containsKey("Ki")) config.Ki = doc["Ki"];
    if (doc.containsKey("Kd")) config.Kd = doc["Kd"];
    if (doc.containsKey("usePWM")) config.usePWM = doc["usePWM"];
    if (doc.containsKey("globalMinTempSet")) config.globalMinTempSet = doc["globalMinTempSet"];
    if (doc.containsKey("globalMaxTempSet")) config.globalMaxTempSet = doc["globalMaxTempSet"];
    if (doc.containsKey("latitude")) config.latitude = doc["latitude"];
    if (doc.containsKey("longitude")) config.longitude = doc["longitude"];
    if (doc.containsKey("weatherModeEnabled")) config.weatherModeEnabled = doc["weatherModeEnabled"];
    if (doc.containsKey("cameraEnabled")) config.cameraEnabled = doc["cameraEnabled"];
	if (doc.containsKey("cameraResolution")) config.cameraResolution = doc["cameraResolution"].as<String>();
    if (doc.containsKey("useLimitTemp")) config.useLimitTemp = doc["useLimitTemp"];
    if (doc.containsKey("ledState")) config.ledState = doc["ledState"];
    if (doc.containsKey("ledBrightness")) config.ledBrightness = doc["ledBrightness"];
    if (doc.containsKey("ledRed")) config.ledRed = doc["ledRed"];
    if (doc.containsKey("ledGreen")) config.ledGreen = doc["ledGreen"];
    if (doc.containsKey("ledBlue")) config.ledBlue = doc["ledBlue"];
    
    if (doc.containsKey("tempCurve") && doc["tempCurve"].is<JsonArray>()) {
        JsonArray curve = doc["tempCurve"].as<JsonArray>();
        for (int i = 0; i < TEMP_CURVE_POINTS && i < curve.size(); i++) {
            config.tempCurve[i] = curve[i].as<float>();
        }
    }

    // Appliquer immédiatement les changements PID
    myPID.SetTunings(config.Kp, config.Ki, config.Kd);

    // Appliquer immédiatement l'état de la LED
    pixels.setBrightness(config.ledBrightness);
    if (config.ledState) {
      pixels.setPixelColor(0, pixels.Color(config.ledRed, config.ledGreen, config.ledBlue));
    } else {
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    }
    pixels.show();
    
    // Demander la sauvegarde
    requestConfigSave();

    Serial.println("[CONF] ✔️ Paramètres appliqués et sauvegardés via /applyAllSettings");
    request->send(200, "text/plain", "Paramètres appliqués et sauvegardés");
}

// === RESOLUTION CAM ===
void handleResolutionSetting(AsyncWebServerRequest *request) {
    if (request->hasParam("quality")) {
        String quality = request->getParam("quality")->value();

        sensor_t *s = esp_camera_sensor_get();

        if (quality == "qvga") {
            s->set_framesize(s, FRAMESIZE_QVGA);
        } else if (quality == "vga") {
            s->set_framesize(s, FRAMESIZE_VGA);
        } else if (quality == "svga") {
            s->set_framesize(s, FRAMESIZE_SVGA);
        } else {
            request->send(400, "text/plain", "Valeur de qualité non valide");
            return;
        }

        Serial.printf("Résolution de la caméra définie sur: %s\n", quality.c_str());
        request->send(200, "text/plain", "Résolution mise à jour");
    } else {
        request->send(400, "text/plain", "Paramètre de qualité manquant");
    }
}


// === AFFICHAGE OLED ===
void renderOLEDPage(int page, int xOffset) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    int hour = timeinfo.tm_hour;
    bool isDay = hour >= 8 && hour < 20;
    bool heatingOn = (output > 0);
    
    switch (page) {
        case 0:
            if (config.weatherModeEnabled) {
                display.printf("Temp Int/Ext %.1f\n",config.setpoint);
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
                display.println("Temp/hum Int");
                display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
                display.setCursor(0, 13);
                display.setTextSize(2);
                display.printf("%.1fC %.1f%%\n", internalTemp, internalHum);
                
                display.setCursor(0, 34);
                display.setTextSize(1);
                display.println("Consigne");
                display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
                display.setCursor(0, 47);
                display.setTextSize(1);
                display.printf("%.0fC %.0f\n", config.setpoint,output);
            }
            break;
            
        case 1:
            if (!config.weatherModeEnabled) {
                display.setTextSize(1);
                display.println("Température max/min");
                display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
                display.setCursor(0, 47);
                display.setTextSize(2);
                display.printf(" T %.1fC C %.1fC\n", config.globalMaxTempSet,config.globalMinTempSet);
                display.printf(heatingOn ? "Chauf On  %.0f\n" : "Chauf Off  %.0f\n",output);
            } 
            
            break;
            
        case 2:
            display.setTextSize(1);
            display.println("Securite");
            display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
            display.setCursor(0, 13);
            display.printf("Niveau: %d\n", safety.currentLevel);
            display.setCursor(0, 25);
            display.printf("Erreurs: %d\n", safety.consecutiveFailures);
            display.setCursor(0, 37);
            display.printf("Config: %s", config.lastSaveTime);
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
    display.clearDisplay();
    renderOLEDPage((displayPage + 1) % pageCount, SCREEN_WIDTH);
    displayPage = (displayPage + 1) % pageCount;
}

void startCamera() {
   // === Caméra ===
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
    cam_config.frame_size = FRAMESIZE_VGA;
    cam_config.jpeg_quality = 10;
    cam_config.fb_count = 1;
    
    esp_err_t err = esp_camera_init(&cam_config);

    if (err != ESP_OK) {
    Serial.printf("❌ Erreur d'init caméra : 0x%x\n", err);
    config.cameraEnabled = false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== Démarrage du système de contrôle de température ===");
    
    startCamera();
    delay(400);

    // === Configuration des LEDs ===
    pixels.begin();
    pixels.show(); // Initialisation, la LED est éteinte

    // === Initialisation des préférences ===
    if (!preferences.begin("system", false)) {
        Serial.println("❌ Échec d'initialisation des préférences !");
    } else {
        Serial.println("✅ Préférences initialisées");
        loadCompleteConfig();  // La configuration est chargée ici
    }

    // Maintenant, appliquer l'état de la LED depuis la configuration chargée
    pixels.setBrightness(config.ledBrightness);
    if (config.ledState) {
      pixels.setPixelColor(0, pixels.Color(config.ledRed, config.ledGreen, config.ledBlue));
    } else {
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    }
    pixels.show();

    // === LittleFS ===
    if (!LittleFS.begin(true)) {
        Serial.println("❌ LittleFS mount failed");
    } else {
        Serial.println("✅ LittleFS mount OK");
    }

    // === WiFi ===
    Serial.print("Connexion au réseau WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ Connecté! IP: " + WiFi.localIP().toString());

    // === Capteurs et périphériques ===
    Wire.begin(I2C_SDA, I2C_SCL);
    pinMode(HEATER_PIN, OUTPUT);
    analogWrite(HEATER_PIN, 10);
    delay(500);
    analogWrite(HEATER_PIN, 0);

    if (!sht31.begin(0x44)) {
        Serial.println("❌ Capteur SHT30 non trouvé !");
        while (true);
    }

    // === OLED ===
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("❌ Échec d'initialisation OLED");
        while (true);
    }
    display.clearDisplay();
    display.setCursor(0, 0);
    display.drawBitmap(positionImageAxeHorizontal, positionImageAxeVertical, logo, largeurDeLimage, hauteurDeLimage, WHITE);
    display.display();

    // === PID ===
    myPID.SetMode(AUTOMATIC);
    myPID.SetOutputLimits(0, 255);
    myPID.SetTunings(config.Kp, config.Ki, config.Kd);

    // === NTP ===
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

    // Watchdog
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);

    // === ROUTES HTTP ===
    // Fichiers statiques
    server.serveStatic("/css/", LittleFS, "/css/").setCacheControl("max-age=86400");
    server.serveStatic("/js/", LittleFS, "/js/");
    server.serveStatic("/images/", LittleFS, "/images/").setDefaultFile("favico.ico");

    // Page d'accueil
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/index.html", "text/html");
        });
    
        // Qualité cam
    
    server.on("/saveConfig", HTTP_GET, handleSaveConfiguration);
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        doc["heaterState"] = output ;
        doc["currentMode"] = config.usePWM ? "PWM" : "ON/OFF";
        doc["consigne"] = config.setpoint;
        doc["usePWM"] = config.usePWM;
        doc["Kp"] = config.Kp;
        doc["Ki"] = config.Ki;
        doc["Kd"] = config.Kd;
        doc["hysteresis"] = config.hysteresis;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.on("/getCurrentConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<2048> doc;
        // Données 
        doc["hysteresis"] = config.hysteresis;
        doc["Kp"] = config.Kp;
        doc["Ki"] = config.Ki;
        doc["Kd"] = config.Kd;
        doc["usePWM"] = config.usePWM;
        doc["globalMinTempSet"] = config.globalMinTempSet;
        doc["globalMaxTempSet"] = config.globalMaxTempSet;
        doc["latitude"] = config.latitude;
        doc["longitude"] = config.longitude;
        doc["weatherModeEnabled"] = config.weatherModeEnabled;
        doc["cameraEnabled"] = config.cameraEnabled;
        doc["cameraResolution"] = config.cameraResolution;
        doc["useLimitTemp"] = config.useLimitTemp;
		
		doc["ledState"] = config.ledState;
		doc["ledBrightness"] = config.ledBrightness;
		doc["ledRed"] = config.ledRed;
		doc["ledGreen"] = config.ledGreen;
		doc["ledBlue"]  = config.ledBlue;
		
		
        // courbe de température
        JsonArray curve = doc.createNestedArray("tempCurve");
        for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
            curve.add(config.tempCurve[i]);
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/listFiles", HTTP_GET, [](AsyncWebServerRequest *request){
        String output = "Fichiers LittleFS:\n";
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
            output += String(file.name()) + " (" + file.size() + " bytes)\n";
            file = root.openNextFile();
        }
        request->send(200, "text/plain", output);
    });

    server.on("/saveProfile", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(2048);
        DeserializationError err = deserializeJson(doc, data);
        if (err) {
            request->send(400, "text/plain", "Erreur JSON");
            return;
        }
        String name = doc["name"];
        JsonArray temps = doc["temperatures"];
        
        File file = LittleFS.open("/" + name + ".json", "w");
        if (!file) {
            request->send(500, "text/plain", "Erreur d'ouverture fichier");
            return;
        }
        serializeJson(doc, file);
        file.close();
        request->send(200, "text/plain", "Profil enregistré !");
    });

    server.on("/listProfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        JsonArray files = doc.to<JsonArray>();

        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            if (name.endsWith(".json")) {
                files.add(name.startsWith("/") ? name.substring(1) : name);
        }
            file = root.openNextFile();
        }

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/deleteProfile", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("name")) {
            request->send(400, "text/plain", "Nom manquant");
            return;
        }
        String filename = "/" + request->getParam("name")->value();
        if (LittleFS.exists(filename)) {
            LittleFS.remove(filename);
            request->send(200, "text/plain", "Profil supprimé");
        } else {
            request->send(404, "text/plain", "Fichier introuvable");
        }
    });

    server.on("/renameProfile", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("from") || !request->hasParam("to")) {
            request->send(400, "text/plain", "Paramètres manquants");
            return;
        }
        String from = "/" + request->getParam("from")->value();
        String to = "/" + request ->getParam("to")->value() + ".json";

        if (!LittleFS.exists(from)) {
            request->send(404, "text/plain", "Fichier source introuvable");
            return;
        }

        if (LittleFS.rename(from, to)) {
            request->send(200, "text/plain", "Profil renommé");
        } else {
            request->send(500, "text/plain", "Erreur lors du renommage");
        }
    });

    server.on("/loadNamedProfile", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("name")) {
        // Serial.println("[DEBUG] Aucun paramètre 'name' reçu.");
            request->send(400, "text/plain", "Nom manquant");
            return;
        }

        String name = request->getParam("name")->value();
        //Serial.println("[DEBUG] Nom brut reçu : " + name);

        if (!name.endsWith(".json")) name += ".json";
        String path = "/" + name;

        //Serial.println("[DEBUG] Chemin fichier : " + path);

        if (!LittleFS.exists(path)) {
            //Serial.println("[DEBUG] ❌ Fichier introuvable : " + path);
            request->send(404, "text/plain", "Profil introuvable");
            return;
        }

        //Serial.println("[DEBUG] ✅ Fichier trouvé, envoi en cours : " + path);
        request->send(LittleFS, path, "application/json");
    });

    server.on("/uploadProfile", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total){
        DynamicJsonDocument doc(2048);
        DeserializationError err = deserializeJson(doc, data);
        if (err) {
            request->send(400, "text/plain", "JSON invalide");
            return;
        }
        String name = doc["name"];
        if (!name.endsWith(".json")) name += ".json";

        File file = LittleFS.open("/" + name, "w");
        if (!file) {
            request->send(500, "text/plain", "Erreur ouverture fichier");
            return;
        }

        serializeJson(doc, file);
        file.close();
        request->send(200, "text/plain", "Profil importé");
    });

    server.on("/downloadProfile", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("name")) {
            request->send(400, "text/plain", "Nom de profil manquant");
            return;
        }
        String name = request->getParam("name")->value();
        String path = "/" + name;
        if (!LittleFS.exists(path)) {
            request->send(404, "text/plain", "Fichier introuvable");
            return;
        }

        AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, "application/json");
        response->addHeader("Content-Disposition", "attachment; filename=" + name);
        request->send(response);
    });

    server.on("/applyAllSettings", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL, handleApplyAllSettings);
    server.on("/setPWMMode", HTTP_GET, handlePWMModeSetting);
    server.on("/setHysteresis", HTTP_GET, handleHysteresisSetting);
    server.on("/setPID", HTTP_GET, handlePIDSetting);
    server.on("/setGlobalMaxTemp", HTTP_GET, handleGlobalTempSettings);
    server.on("/setGlobalMinTemp", HTTP_GET, handleGlobalTempSettings);
	server.on("/set-resolution-cam", HTTP_GET, handleResolutionSetting);
    server.on("/setCamera", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("enabled")) {
            config.cameraEnabled = request->getParam("enabled")->value().toInt() == 1;
            Serial.printf("[TEMP] Caméra: %s (non sauvegardé)\n", config.cameraEnabled ? "activée" : "désactivée");
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Paramètre manquant");
        }
    });
    
    server.on("/setWeather", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("lat") && request->hasParam("lon") && request->hasParam("enabled")) {
            config.latitude = request->getParam("lat")->value().toFloat();
            config.longitude = request->getParam("lon")->value().toFloat();
            config.weatherModeEnabled = request->getParam("enabled")->value().toInt() == 1;
            
            Serial.printf("[TEMP] Météo: enabled=%s, lat=%.4f, lon=%.4f (non sauvegardé)\n", 
                         config.weatherModeEnabled ? "true" : "false", config.latitude, config.longitude);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Paramètres manquants");
        }
    });

    server.on("/setTempCurveMode", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("enabled")) {
            config.useTempCurve = request->getParam("enabled")->value() == "1";
            Serial.printf("[TEMP] Mode courbe de température : %s (non sauvegardé)\n", 
                         config.useTempCurve ? "activé" : "désactivé");
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Paramètre manquant");
        }
    });
    
    server.on("/setLimitTemp", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("enabled")) {
            config.useLimitTemp = request->getParam("enabled")->value() == "1";
            Serial.printf("Limite de température %s\n", 
                         config.useLimitTemp ? "activé" : "désactivé");
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Paramètre manquant");
        }
    });
    // === ROUTES DE LECTURE ===
    server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;
        
        doc["hysteresis"] = config.hysteresis;
        doc["Kp"] = config.Kp;
        doc["Ki"] = config.Ki;
        doc["Kd"] = config.Kd;
        doc["weatherMode"] = config.weatherModeEnabled;
        doc["usePWM"] = config.usePWM;
        doc["latitude"] = config.latitude;
        doc["longitude"] = config.longitude;
        doc["cameraEnabled"] = config.cameraEnabled;
		doc["cameraResolution"] = config.cameraResolution;
        doc["useTempCurve"] = config.useTempCurve;
        doc["globalMinTempSet"] = config.globalMinTempSet;
        doc["globalMaxTempSet"] = config.globalMaxTempSet;
        doc["lastSave"] = config.lastSaveTime;
        doc["ledState"] = config.ledState;
		doc["ledBrightness"] = config.ledBrightness;
		doc["ledRed"] = config.ledRed;
		doc["ledGreen"] = config.ledGreen;
		doc["ledBlue"]  = config.ledBlue;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    server.on("/getTempCurve", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        JsonArray array = doc.to<JsonArray>();
        
        for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
            array.add(config.tempCurve[i]);
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // === ROUTES DE DONNÉES ===
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
    
    server.on("/weatherData", HTTP_GET, [](AsyncWebServerRequest *request) {
        ExternalWeather w = getWeatherData();
        if (isnan(w.temperature) || isnan(w.humidity)) {
            request->send(503, "application/json", "{\"error\":\"invalid data\"}");
        } else {
            String json = "{\"temp\":" + String(w.temperature, 1) + 
                         ",\"hum\":" + String(w.humidity, 0) + "}";
            request->send(200, "application/json", json);
        }
    });
    
    // === SÉCURITÉ ===
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
    
    // === CAMÉRA ===
    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            request->send(500, "text/plain", "Erreur capture image");
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
            }
        );
        
        response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
        request->send(response);
    });
    
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(204);
    });
    
	server.on("/updateLed", HTTP_GET, [](AsyncWebServerRequest *request) {

    if (request->hasParam("state")) {
      config.ledState = request->getParam("state")->value() == "true";
      
    }
    if (request->hasParam("brightness")) {
      config.ledBrightness = request->getParam("brightness")->value().toInt();
      
    }
    if (request->hasParam("red")) {
      config.ledRed = request->getParam("red")->value().toInt();
      
    }
    if (request->hasParam("green")) {
      config.ledGreen = request->getParam("green")->value().toInt();
      
    }
    if (request->hasParam("blue")) {
      config.ledBlue = request->getParam("blue")->value().toInt();
      
    }

    if (config.ledState) {
      pixels.setPixelColor(0, pixels.Color(config.ledRed, config.ledGreen, config.ledBlue));
    } else {
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    }
    pixels.setBrightness(config.ledBrightness);
    pixels.show();

    request->send(200, "text/plain", "OK");
  });

  server.on("/getStateLed", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    doc["state"] = config.ledState;
    doc["brightness"] = config.ledBrightness;
    doc["red"] = config.ledRed;
    doc["green"] = config.ledGreen;
    doc["blue"] = config.ledBlue;

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
    // === 404 ===	
    server.onNotFound([](AsyncWebServerRequest *request) {
        Serial.printf("❌ Route non trouvée : %s\n", request->url().c_str());
        request->send(404, "text/plain", "Not found");
    });
    
    // === Démarrage serveur ===
   server.begin();
    delay(300);
    Serial.println("✅ Serveur HTTP démarré");
    
    // Affichage de la configuration
    Serial.println("===================================");
    Serial.printf("→ Configuration chargée:\n");
    Serial.printf("  cameraEnabled: %s\n", config.cameraEnabled ? "true" : "false");
	Serial.printf("  cameraResolution: %s\n", config.cameraResolution);
    Serial.printf("  useTempCurve: %s\n", config.useTempCurve ? "true" : "false");
    Serial.printf("  PID: Kp=%.1f, Ki=%.1f, Kd=%.1f\n", config.Kp, config.Ki, config.Kd);
    Serial.printf("  usePWM: %s\n", config.usePWM ? "true" : "false");
    Serial.printf("  Hystérésis: %.1f\n", config.hysteresis);
    Serial.printf("  Coord GPS: %.4f, %.4f\n", config.latitude, config.longitude);
    Serial.printf("  useLimitTemp: %s\n", config.useLimitTemp ? "true" : "false");
    Serial.printf("  weatherMode: %s\n", config.weatherModeEnabled ? "true" : "false");
    Serial.printf("  ledState: %s\n", config.ledState ? "true" : "false");
    Serial.printf("  ledBrightness: %d\n", config.ledBrightness);
    Serial.printf("  ledRed: %d\n", config.ledRed);
    Serial.printf("  ledGreen: %d\n", config.ledGreen);
    Serial.printf("  ledBlue: %d\n", config.ledBlue);
    Serial.println("  Courbe de température : ");
    for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
        Serial.printf("     Heure: %d Température: %.1f °C\n", i, config.tempCurve[i]);
    }
    Serial.println("===================================");
    Serial.printf("  Dernière sauvegarde: %s\n", config.lastSaveTime);
    Serial.println("===================================");
}

// === LOOP ===
void loop() {
    esp_task_wdt_reset();
    
    static unsigned long lastUpdate = 0;
    static unsigned long lastSafetyCheck = 0;

    // Vérification de sécurité toutes les 10 secondes
    if (millis() - lastSafetyCheck > 10000) {
        checkSafetyConditions();
        lastSafetyCheck = millis();
    }

    // Mise à jour normale toutes les 5 secondes
    if (millis() - lastUpdate > 5000) {
        updateInternalSensor();
        slideToNextPage();
        lastUpdate = millis();
    }

    // Gestion de l'affichage d'urgence (clignotement)
    if (safety.currentLevel == SAFETY_EMERGENCY) {
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 1000) {
            activateEmergencyMode(safety.lastErrorMessage);
            lastBlink = millis();
        }
    }

    // Mise à jour météo
    if (config.weatherModeEnabled && millis() - lastWeatherUpdate > weatherInterval) {
        lastWeatherUpdate = millis();
        ExternalWeather ext = getWeatherData();
        if (!isnan(ext.temperature)) {
            externalTemp = ext.temperature;
            config.setpoint = externalTemp;
            Serial.printf("Setpoint météo mis à %.1f°C \n", config.setpoint);
        }
        if (!isnan(ext.humidity)) {
            externalHum = ext.humidity;
        }
    }

    // Sauvegarde différée
    if (savePending && millis() - lastSaveRequest > saveDelay) {
        saveConfigIfChanged();
        savePending = false;
    }

    delay(500);
}
