#include "WebServer.h"
#include "../config/ConfigManager.h"
#include "../sensors/SensorManager.h"
#include "../sensors/SafetySystem.h"
#include "../utils/Logger.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// Références aux variables globales (définies dans main.cpp)
extern SystemConfig config;
extern SafetySystem safety;
extern HistoryRecord history[];
extern int historyIndex;
extern bool historyFull;
extern double output;

void WebServerManager::setupRoutes(AsyncWebServer& server) {
    // === FICHIERS STATIQUES ===
    server.serveStatic("/css/", LittleFS, "/css/").setCacheControl("max-age=86400");
    server.serveStatic("/js/", LittleFS, "/js/");
    server.serveStatic("/images/", LittleFS, "/images/");
    
    // === PAGE PRINCIPALE ===
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    // === CONFIGURATION ===
    server.on("/getCurrentConfig", HTTP_GET, handleGetCurrentConfig);
    server.on("/applyAllSettings", HTTP_POST, 
              [](AsyncWebServerRequest *req){}, NULL, handleApplyAllSettings);
    server.on("/saveConfig", HTTP_GET, handleSaveConfiguration);
    
    // === PROFILS ===
    server.on("/listProfiles", HTTP_GET, handleListProfiles);
    server.on("/loadProfile", HTTP_GET, handleLoadProfile);
    server.on("/saveProfile", HTTP_POST, 
              [](AsyncWebServerRequest *req){}, NULL, handleSaveProfile);
    server.on("/deleteProfile", HTTP_DELETE, handleDeleteProfile);
    server.on("/activateProfile", HTTP_GET, handleActivateProfile);
    
    // === DONNÉES SAISONNIÈRES ===
    server.on("/getDayData", HTTP_GET, handleGetDayData);
    server.on("/saveDayData", HTTP_POST, 
              [](AsyncWebServerRequest *req){}, NULL, handleSaveDayData);
    server.on("/getYearlyTemperatures", HTTP_GET, handleGetYearlyTemperatures);
    
    // === DONNÉES TEMPS RÉEL ===
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String((float)SensorManager::getCurrentTemperature() / 10.0f, 1));
    });
    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(SensorManager::getCurrentHumidity(), 1));
    });
    server.on("/heaterState", HTTP_GET, [](AsyncWebServerRequest *request) {
        int percentage = map(output, 0, 255, 0, 100);
        request->send(200, "text/plain", String(percentage));
    });
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/history", HTTP_GET, handleHistory);
    server.on("/safetyStatus", HTTP_GET, handleSafetyStatus);
    
    // === CAMÉRA ===
    server.on("/capture", HTTP_GET, handleCapture);
    server.on("/stream", HTTP_GET, handleVideoStream);
    server.on("/mjpeg", HTTP_GET, handleMJPEG);
    
    // === DEBUG / LOGGING ===
    server.on("/setLogLevel", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("level")) {
            int level = request->getParam("level")->value().toInt();
            if (level >= LOG_LEVEL_NONE && level <= LOG_LEVEL_DEBUG) {
                config.logLevel = level;
                setLogLevel((LogLevel)level);
                ConfigManager::requestSave();
                String msg = "Log level set to " + String(logLevelToString((LogLevel)level));
                request->send(200, "text/plain", msg);
                LOG_INFO("WEBSERVER", "Niveau de log changé à %d via API", level);
            } else {
                request->send(400, "text/plain", "Invalid log level");
            }
        } else {
            request->send(400, "text/plain", "Missing 'level' parameter");
        }
    });
    
    LOG_INFO("WEBSERVER", "Routes web configurées");
}

void WebServerManager::handleGetCurrentConfig(AsyncWebServerRequest *request) {
    StaticJsonDocument<2048> doc;
    
    // Paramètres de contrôle
    doc["usePWM"] = config.usePWM;
    doc["weatherModeEnabled"] = config.weatherModeEnabled;
    doc["currentProfileName"] = config.currentProfileName;
    doc["cameraEnabled"] = config.cameraEnabled;
    doc["cameraResolution"] = config.cameraResolution;
    doc["useTempCurve"] = config.useTempCurve;
    doc["useLimitTemp"] = config.useLimitTemp;
    
    // PID (float)
    doc["hysteresis"] = config.hysteresis;
    doc["Kp"] = config.Kp;
    doc["Ki"] = config.Ki;
    doc["Kd"] = config.Kd;
    
    // GPS (float)
    doc["latitude"] = config.latitude;
    doc["longitude"] = config.longitude;
    doc["DST_offset"] = config.DST_offset;
    
    // Températures (conversion int16 → float)
    doc["setpoint"] = config.getSetpointFloat();
    doc["globalMinTempSet"] = config.getMinTempFloat();
    doc["globalMaxTempSet"] = config.getMaxTempFloat();
    
    // Courbe 24h
    JsonArray curve = doc.createNestedArray("tempCurve");
    for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
        curve.add(config.getTempCurve(i));
    }
    
    // LED
    doc["ledState"] = config.ledState;
    doc["ledBrightness"] = config.ledBrightness;
    doc["ledRed"] = config.ledRed;
    doc["ledGreen"] = config.ledGreen;
    doc["ledBlue"] = config.ledBlue;
    
    // Métadonnées
    doc["seasonalModeEnabled"] = config.seasonalModeEnabled;
    doc["logLevel"] = config.logLevel;
    doc["lastSaveTime"] = config.lastSaveTime;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleApplyAllSettings(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        request->send(400, "text/plain", "JSON invalide");
        return;
    }
    
    if (!validateJsonConfig(doc)) {
        request->send(400, "text/plain", "Configuration invalide");
        return;
    }
    
    // Mise à jour de la configuration
    if (doc.containsKey("currentProfileName")) 
        config.currentProfileName = doc["currentProfileName"].as<String>();
    if (doc.containsKey("usePWM")) 
        config.usePWM = doc["usePWM"];
    if (doc.containsKey("weatherModeEnabled")) 
        config.weatherModeEnabled = doc["weatherModeEnabled"];
    if (doc.containsKey("cameraEnabled")) 
        config.cameraEnabled = doc["cameraEnabled"];
    if (doc.containsKey("cameraResolution")) 
        config.cameraResolution = doc["cameraResolution"].as<String>();
    if (doc.containsKey("useTempCurve")) 
        config.useTempCurve = doc["useTempCurve"];
    if (doc.containsKey("useLimitTemp")) 
        config.useLimitTemp = doc["useLimitTemp"];
    
    // PID
    if (doc.containsKey("hysteresis")) 
        config.hysteresis = doc["hysteresis"];
    if (doc.containsKey("Kp")) 
        config.Kp = doc["Kp"];
    if (doc.containsKey("Ki")) 
        config.Ki = doc["Ki"];
    if (doc.containsKey("Kd")) 
        config.Kd = doc["Kd"];
    
    // GPS
    if (doc.containsKey("latitude")) 
        config.latitude = doc["latitude"];
    if (doc.containsKey("longitude")) 
        config.longitude = doc["longitude"];
    if (doc.containsKey("DST_offset")) 
        config.DST_offset = doc["DST_offset"];
    
    // Températures (conversion float → int16)
    if (doc.containsKey("setpoint")) 
        config.setSetpointValue(doc["setpoint"]);
    if (doc.containsKey("globalMinTempSet")) 
        config.setMinTemp(doc["globalMinTempSet"]);
    if (doc.containsKey("globalMaxTempSet")) 
        config.setMaxTemp(doc["globalMaxTempSet"]);
    
    // Courbe 24h
    if (doc.containsKey("tempCurve") && doc["tempCurve"].is<JsonArray>()) {
        JsonArray curve = doc["tempCurve"].as<JsonArray>();
        for (int i = 0; i < TEMP_CURVE_POINTS && i < curve.size(); i++) {
            config.setTempCurve(i, curve[i]);
        }
    }
    
    // LED
    if (doc.containsKey("ledState")) 
        config.ledState = doc["ledState"];
    if (doc.containsKey("ledBrightness")) 
        config.ledBrightness = doc["ledBrightness"];
    if (doc.containsKey("ledRed")) 
        config.ledRed = doc["ledRed"];
    if (doc.containsKey("ledGreen")) 
        config.ledGreen = doc["ledGreen"];
    if (doc.containsKey("ledBlue")) 
        config.ledBlue = doc["ledBlue"];
    
    // Métadonnées
    if (doc.containsKey("seasonalModeEnabled")) 
        config.seasonalModeEnabled = doc["seasonalModeEnabled"];
    if (doc.containsKey("logLevel")) 
        config.logLevel = doc["logLevel"];
    
    // Appliquer les changements (sera fait dans la tâche principale)
    ConfigManager::requestSave();
    
    LOG_INFO("WEBSERVER", "Configuration mise à jour via API");
    request->send(200, "text/plain", "Configuration appliquée");
}

void WebServerManager::handleListProfiles(AsyncWebServerRequest *request) {
    std::vector<String> profileNames = ConfigManager::listProfiles();
    
    DynamicJsonDocument doc(2048);
    JsonArray profiles = doc.createNestedArray("profiles");
    
    for (const String& name : profileNames) {
        JsonObject profile = profiles.createNestedObject();
        profile["name"] = name;
        profile["hasGeneral"] = true;
        
        // Informations sur les fichiers
        String generalPath = "/profiles/" + name + "/general.json";
        String tempPath = "/profiles/" + name + "/temperature.bin";
        
        if (LittleFS.exists(generalPath)) {
            File genFile = LittleFS.open(generalPath, "r");
            profile["generalSize"] = genFile.size();
            genFile.close();
        }
        
        if (LittleFS.exists(tempPath)) {
            File tempFile = LittleFS.open(tempPath, "r");
            profile["tempSize"] = tempFile.size();
            profile["hasTempData"] = true;
            tempFile.close();
        } else {
            profile["hasTempData"] = false;
        }
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleGetDayData(AsyncWebServerRequest *request) {
    if (!request->hasParam("day")) {
        request->send(400, "text/plain", "Paramètre 'day' manquant");
        return;
    }
    
    int dayIndex = request->getParam("day")->value().toInt();
    if (dayIndex < 0 || dayIndex >= 366) {
        request->send(400, "text/plain", "Jour invalide (0-365)");
        return;
    }
    
    float dayTemperatures[24];
    if (ConfigManager::loadSeasonalData(config.currentProfileName, dayIndex, dayTemperatures)) {
        StaticJsonDocument<1024> doc;
        JsonArray temps = doc.createNestedArray("temperatures");
        
        for (int i = 0; i < 24; i++) {
            temps.add(dayTemperatures[i]);
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(500, "text/plain", "Erreur de lecture des données du jour");
    }
}

void WebServerManager::handleStatus(AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    
    doc["heaterState"] = (int)output;
    doc["currentMode"] = config.usePWM ? "PWM" : "ON/OFF";
    doc["setpoint"] = config.getSetpointFloat();
    doc["temperature"] = (float)SensorManager::getCurrentTemperature() / 10.0f; // Convert int16_t to float
    doc["humidity"] = SensorManager::getCurrentHumidity();
    doc["sensorValid"] = SensorManager::isDataValid();
    doc["safetyLevel"] = safety.currentLevel;
    doc["emergencyShutdown"] = safety.emergencyShutdown;
    doc["lastUpdate"] = SensorManager::getLastUpdateTime();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleSafetyStatus(AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    
    doc["level"] = safety.currentLevel;
    doc["levelName"] = (safety.currentLevel == SAFETY_NORMAL) ? "Normal" : 
                      (safety.currentLevel == SAFETY_WARNING) ? "Alerte" : 
                      (safety.currentLevel == SAFETY_CRITICAL) ? "Critique" : "Urgence";
    doc["emergencyShutdown"] = safety.emergencyShutdown;
    doc["lastError"] = safety.lastErrorMessage;
    doc["consecutiveFailures"] = safety.consecutiveFailures;
    doc["lastSensorRead"] = (millis() - safety.lastSensorRead) / 1000;
    doc["lastKnownTemp"] = (float)safety.lastKnownGoodTemp / 10.0f; // Convert int16_t to float
    doc["lastKnownHum"] = safety.lastKnownGoodHum;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

bool WebServerManager::validateJsonConfig(const DynamicJsonDocument& doc) {
    // Validation des paramètres critiques
    if (doc.containsKey("globalMinTempSet") && doc.containsKey("globalMaxTempSet")) {
        float minTemp = doc["globalMinTempSet"];
        float maxTemp = doc["globalMaxTempSet"];
        if (minTemp >= maxTemp || minTemp < 0 || maxTemp > 50) {
            return false;
        }
    }
    
    if (doc.containsKey("hysteresis")) {float hysteresis = doc["hysteresis"];
if (hysteresis <= 0 || hysteresis > 10) {
return false;
}
}
if (doc.containsKey("Kp") || doc.containsKey("Ki") || doc.containsKey("Kd")) {
    float kp = doc.containsKey("Kp") ? doc["Kp"].as<float>() : config.Kp;
    float ki = doc.containsKey("Ki") ? doc["Ki"].as<float>() : config.Ki;
    float kd = doc.containsKey("Kd") ? doc["Kd"].as<float>() : config.Kd;
    
    if (kp < 0 || kp > 100 || ki < 0 || ki > 100 || kd < 0 || kd > 100) {
        return false;
    }
}

return true;

}
void WebServerManager::handleSaveConfiguration(AsyncWebServerRequest *request) {
ConfigManager::requestSave();
request->send(200, "text/plain", "Sauvegarde demandée");
}
void WebServerManager::handleHistory(AsyncWebServerRequest *request) {
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
            ",\"temp\":