#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "config/SystemConfig.h"
#include "config/ConfigManager.h"
#include "sensors/SensorManager.h"
#include "sensors/SafetySystem.h"
#include "utils/Logger.h"
#include "web/AppWebServer.h"
#include "wifi_credentials.h"

// === INCLUDES MATÉRIELS ===
#include <WiFi.h>
#include <time.h>
#include <PID_v1.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>





// === CONFIGURATION MATÉRIELLE ===
using namespace HardwareConstants;

// === VARIABLES GLOBALES ===
SystemConfig config;

// Variables de mesure
int16_t internalTemp = 0;
float internalHum = NAN;
float externalTemp = 0.0f;
float externalHum = 0.0f;

// Contrôle chauffage
bool heaterState = false;
double input, output = 0;
unsigned long lastToggleTime = 0;
bool manualCycleOn = false;

// Historique
HistoryRecord history[MAX_HISTORY_RECORDS];
int historyIndex = 0;
bool historyFull = false;

// Statistiques (simplifiées)
int16_t maxTemperature = -32768;
int16_t minTemperature = 32767;

// Objets matériels
AsyncWebServer server(80);
PID myPID(&input, &output, (double*)&config.setpoint, config.Kp, config.Ki, config.Kd, DIRECT);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_NeoPixel pixels(NUMPIXELS, APP_PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Synchronisation
SemaphoreHandle_t i2cMutex = NULL;
TaskHandle_t Core1TaskHandle = NULL;

// Affichage OLED
unsigned long lastDisplayUpdate = 0;
int displayPage = 0;
const int pageCount = 4;

// === DÉCLARATIONS DE FONCTIONS ===
void mainApplicationTask(void *pvParameters);
void initFileSystem();
void initHardware();
void initNetworking();
void initSensors();
void initWebServer();
void initTasks();

int16_t getCurrentTargetTemperature();
void controlHeater(int16_t currentTemperature);
void addToHistory(int16_t temperature, float humidity);
void renderOLEDPage(int page);
bool updateDisplaySafe();

// === SETUP PRINCIPAL ===
void setup() {
    Serial.begin(115200);
    delay(2000);
    delay(10000); // Pause de 10 secondes pour les logs
    
    LOG_INFO("MAIN", "==================================================");
    LOG_INFO("MAIN", "🚀 CONTRÔLEUR VIVARIUM v2.0 REFACTORISÉ");
    LOG_INFO("MAIN", "==================================================");
    
    initFileSystem();
    LOG_INFO("MAIN", "Current profile name: %s", config.currentProfileName.c_str());
    initHardware();
    initNetworking();
    initSensors();
    initWebServer();
    initTasks();
    
    LOG_INFO("MAIN", "==================================================");
    LOG_INFO("MAIN", "✅ SYSTÈME INITIALISÉ AVEC SUCCÈS !");
    LOG_INFO("MAIN", "==================================================");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ========================================
// FONCTIONS D'INITIALISATION MODULAIRES
// ========================================

void initFileSystem() {
    LOG_INFO("FILESYSTEM", "Initialisation...");
    if (!ConfigManager::initialize()) {
        LOG_ERROR("FILESYSTEM", "Échec initialisation ConfigManager");
        return;
    }
    if (!ConfigManager::loadConfig(config)) {
        LOG_ERROR("FILESYSTEM", "Échec chargement configuration");
        return;
    }
    setLogLevel((LogLevel)config.logLevel);
    LOG_INFO("FILESYSTEM", "Initialisation réussie.");
}

void initHardware() {
    LOG_INFO("HARDWARE", "Initialisation...");
    i2cMutex = xSemaphoreCreateMutex();
    if (i2cMutex == NULL) {
        LOG_ERROR("HARDWARE", "Échec création mutex I2C");
        while(1);
    }
    pinMode(HEATER_PIN, OUTPUT);
    analogWrite(HEATER_PIN, 0);
    Wire.begin(I2C_SDA, I2C_SCL);
    pixels.begin();
    pixels.setBrightness(config.ledBrightness);
    if (config.ledState) {
        pixels.setPixelColor(0, pixels.Color(config.ledRed, config.ledGreen, config.ledBlue));
    } else {
        pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    }
    pixels.show();
    myPID.SetMode(AUTOMATIC);
    myPID.SetOutputLimits(0, 255);
    myPID.SetTunings(config.Kp, config.Ki, config.Kd);
    LOG_INFO("HARDWARE", "Initialisation réussie.");
}

void initNetworking() {
    LOG_INFO("NETWORK", "Initialisation...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int wifi_retries = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_retries < 20) {
        delay(500);
        Serial.print(".");
        wifi_retries++;
    }
    Serial.println();
    if(WiFi.status() == WL_CONNECTED){
        LOG_INFO("NETWORK", "WiFi connecté. IP: %s", WiFi.localIP().toString().c_str());
    } else {
        LOG_ERROR("NETWORK", "Échec de la connexion WiFi.");
    }
    configTime(3600, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        LOG_INFO("NETWORK", "Heure synchronisée (NTP).");
    } else {
        LOG_WARN("NETWORK", "Échec synchronisation heure (NTP).");
    }
}

void initSensors() {
    LOG_INFO("SENSORS", "Initialisation...");
    SensorManager::setI2CMutex(i2cMutex);
    if (!SensorManager::initialize()) {
        LOG_ERROR("SENSORS", "Échec initialisation SensorManager");
        return;
    }
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        LOG_ERROR("SENSORS", "Échec initialisation OLED");
        return;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("VIVARIUM v2.0");
    display.setCursor(0, 16);
    display.println("Initialisation...");
    display.display();
    SafetySystem::initialize();
    LOG_INFO("SENSORS", "Initialisation réussie.");
}

void initWebServer() {
    LOG_INFO("WEBSERVER", "Initialisation...");
    AppWebServerManager::setupRoutes(server);
    server.begin();
    LOG_INFO("WEBSERVER", "Serveur démarré sur http://%s", WiFi.localIP().toString().c_str());
}

void initTasks() {
    LOG_INFO("TASKS", "Création des tâches...");
    esp_task_wdt_init(30, true);
    xTaskCreatePinnedToCore(
        mainApplicationTask,
        "MainApp",
        10000,
        NULL,
        1,
        &Core1TaskHandle,
        1
    );
    if (Core1TaskHandle == NULL) {
        LOG_ERROR("TASKS", "Échec création tâche principale");
        return;
    }
    LOG_INFO("TASKS", "Tâches créées avec succès.");
}

// ========================================
// TÂCHE PRINCIPALE (CORE 1)
// ========================================

void mainApplicationTask(void *pvParameters) {
    LOG_INFO("TASKS", "Tâche principale démarrée sur Core 1");
    esp_task_wdt_add(NULL);
    unsigned long lastSensorUpdate = 0;
    unsigned long lastDisplayUpdate = 0;
    unsigned long lastPageChange = 0;
    unsigned long lastHistoryUpdate = 0;
    
    for (;;) {
        esp_task_wdt_reset();
        unsigned long now = millis();
        
        if (now - lastSensorUpdate >= 2000) {
            lastSensorUpdate = now;
            if (SensorManager::updateSensors()) {
                internalTemp = SensorManager::getCurrentTemperature();
                internalHum = SensorManager::getCurrentHumidity();
                if (internalTemp > maxTemperature) maxTemperature = internalTemp;
                if (internalTemp < minTemperature) minTemperature = internalTemp;
                controlHeater(internalTemp);
                if (now - lastHistoryUpdate >= 60000) {
                    lastHistoryUpdate = now;
                    addToHistory(internalTemp, internalHum);
                }
            }
            SafetySystem::checkConditions(internalTemp, internalHum);
        }
        
        ConfigManager::processPendingSave(config);
        
        if (now - lastDisplayUpdate >= 1000) {
            lastDisplayUpdate = now;
            if (SafetySystem::getCurrentLevel() == SAFETY_NORMAL) {
                renderOLEDPage(displayPage);
                updateDisplaySafe();
            }
        }
        
        if (now - lastPageChange >= 10000) {
            lastPageChange = now;
            if (SafetySystem::getCurrentLevel() == SAFETY_NORMAL) {
                displayPage = (displayPage + 1) % pageCount;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ========================================
// FONCTIONS DE CONTRÔLE
// ========================================

int16_t getCurrentTargetTemperature() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return config.setpoint;
    }
    return config.getTempCurve(timeinfo.tm_hour);
}

void controlHeater(int16_t currentTemperature) {
    if (SafetySystem::isEmergencyShutdown() || SafetySystem::getCurrentLevel() >= SAFETY_CRITICAL) {
        output = 0;
        analogWrite(HEATER_PIN, 0);
        //LOG_WARN("HEATER", "Chauffage bloqué par le système de sécurité (Niveau: %d)", SafetySystem::getCurrentLevel());
        return;
    }
    
    int16_t targetTemp = getCurrentTargetTemperature();
    int16_t maxTemp = targetTemp;
    int16_t minTemp = targetTemp - (int16_t)(config.hysteresis * 10);
    unsigned long now = millis();
    
    if (currentTemperature >= maxTemp) {
        output = 0;
        manualCycleOn = false;
    } else if (currentTemperature < minTemp) {
        output = 255;
        manualCycleOn = false;
    } else {
        if (config.usePWM) {
            input = (double)currentTemperature / 10.0;
            myPID.SetTunings(config.Kp, config.Ki, config.Kd);
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
    
    if (SafetySystem::getCurrentLevel() == SAFETY_WARNING) {
        output = min(output, 128.0);
    }
    
    analogWrite(HEATER_PIN, output);
}

void addToHistory(int16_t temperature, float humidity) {
    time_t now = time(nullptr);
    history[historyIndex] = { now, (float)temperature, humidity };
    historyIndex = (historyIndex + 1) % MAX_HISTORY_RECORDS;
    if (historyIndex == 0) historyFull = true;
    LOG_DEBUG("HISTORY", "Historique mis à jour: %.1f°C, %.0f%%", (float)temperature / 10.0f, humidity);
}

void renderOLEDPage(int page) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    
    switch (page) {
        case 0:
            display.printf("Temp: %.1fC (%.1f)\n", (float)internalTemp / 10.0f, (float)getCurrentTargetTemperature() / 10.0f);
            display.printf("Hum:  %.0f%%\n", internalHum);
            display.printf("Chauf: %s (%.0f)\n", output > 0 ? "ON" : "OFF", output);
            display.printf("Mode: %s\n", config.usePWM ? "PWM" : "ON/OFF");
            display.printf("Prof: %s", config.currentProfileName.c_str());
            break;
        case 1:
            display.println("STATISTIQUES");
            display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
            display.printf("T Max: %.1fC\n", (float)maxTemperature / 10.0f);
            display.printf("T Min: %.1fC\n", (float)minTemperature / 10.0f);
            display.printf("Capteur: %s\n", SensorManager::isDataValid() ? "OK" : "ERR");
            display.printf("Securite: %d", SafetySystem::getCurrentLevel());
            break;
        case 2:
            display.println("SYSTEME");
            display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
            display.printf("Heure: %s\n", timeStr);
            display.printf("WiFi: %s\n", WiFi.isConnected() ? "OK" : "ERR");
            display.printf("Config: v%d", config.configVersion);
            break;
        case 3:
            display.println("MODES");
            display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
            display.printf("PWM: %s\n", config.usePWM ? "ON" : "OFF");
            display.printf("Meteo: %s\n", config.weatherModeEnabled ? "ON" : "OFF");
            display.printf("Camera: %s", config.cameraEnabled ? "ON" : "OFF");
            break;
    }
}

bool updateDisplaySafe() {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    display.display();
    xSemaphoreGive(i2cMutex);
    return true;
}

// ========================================
// ACCESSEURS DE DONNÉES POUR L'API WEB
// ========================================

SystemConfig& getGlobalConfig() {
    return config;
}

double getHeaterOutput() {
    return output;
}

HistoryRecord* getHistory() {
    return history;
}

int getHistoryIndex() {
    return historyIndex;
}

bool isHistoryFull() {
    return historyFull;
}