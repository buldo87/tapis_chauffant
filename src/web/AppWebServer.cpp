#include "AppWebServer.h"
#include "../config/SystemConfig.h"
#include "../config/ConfigManager.h"
#include "../sensors/SensorManager.h"
#include "../sensors/SafetySystem.h"
#include "../utils/Logger.h"
#include "../hardware/CameraManager.h" // Ajout de l'en-tête
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LittleFS.h>

// Forward declarations for functions in main.cpp
SystemConfig& getGlobalConfig();
double getHeaterOutput();
HistoryRecord* getHistory();
int getHistoryIndex();
bool isHistoryFull();
int16_t getCurrentTargetTemperature();

void AppWebServerManager::setupRoutes(AsyncWebServer& server) {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/css/style.css", "text/css");
    });

    server.on("/js/main.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/main.js", "application/javascript");
    });
    server.on("/js/api.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/api.js", "application/javascript");
    });
    server.on("/js/state.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/state.js", "application/javascript");
    });
    server.on("/js/utils.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/utils.js", "application/javascript");
    });
    server.on("/js/ui/camera.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/ui/camera.js", "application/javascript");
    });
    server.on("/js/ui/configuration.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/ui/configuration.js", "application/javascript");
    });
    server.on("/js/ui/led.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/ui/led.js", "application/javascript");
    });
    server.on("/js/ui/map.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/ui/map.js", "application/javascript");
    });
    server.on("/js/ui/profiles.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/ui/profiles.js", "application/javascript");
    });
    server.on("/js/ui/seasonal.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/ui/seasonal.js", "application/javascript");
    });
    server.on("/js/ui/surveillance.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/ui/surveillance.js", "application/javascript");
    });
    server.on("/js/ui/tabs.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/js/ui/tabs.js", "application/javascript");
    });
     server.on("/images/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/images/favicon.ico", "image/x-icon");
    });

    server.on("/api/config", HTTP_GET, handleGetCurrentConfig);
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL, handleApplyAllSettings);
    server.on("/api/save", HTTP_POST, handleSaveConfiguration);

    server.on("/api/profiles", HTTP_GET, handleListProfiles);
    server.on("/api/profiles/load", HTTP_GET, handleLoadProfile);
    server.on("/api/profiles/save", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL, handleSaveProfile);
    server.on("/api/profiles/delete", HTTP_POST, handleDeleteProfile);
    server.on("/api/profiles/activate", HTTP_POST, handleActivateProfile);

    server.on("/api/seasonal/day", HTTP_GET, handleGetDayData);
    server.on("/api/seasonal/day", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL, handleSaveDayData);
    server.on("/api/seasonal/yearly", HTTP_GET, handleGetYearlyTemperatures);
    server.on("/api/seasonal/extend", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL, handleExtendMonthData);
    server.on("/api/seasonal/smooth", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL, handleSmoothMonthData);
    server.on("/api/applyYearlyCurve", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL, handleApplyYearlyCurve);

    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/history", HTTP_GET, handleHistory);
    server.on("/api/safety", HTTP_GET, handleSafetyStatus);

    server.on("/capture", HTTP_GET, CameraManager::handleCapture);
    server.on("/mjpeg", HTTP_GET, CameraManager::handleStream);
    
    
    server.on("/download/profile", HTTP_GET, handleDownloadProfile);
    server.on("/download/seasonal", HTTP_GET, handleDownloadSeasonalData);
    server.on("/api/camera/set", HTTP_POST, handleSetCamera);
}

void AppWebServerManager::handleGetCurrentConfig(AsyncWebServerRequest *request) {
    SystemConfig& config = getGlobalConfig();
    DynamicJsonDocument doc(2048);
    doc["currentProfileName"] = config.currentProfileName;
    doc["usePWM"] = config.usePWM;
    doc["weatherModeEnabled"] = config.weatherModeEnabled;
    doc["cameraEnabled"] = config.cameraEnabled;
    doc["cameraResolution"] = config.cameraResolution;
    doc["useTempCurve"] = config.useTempCurve;
    doc["useLimitTemp"] = config.useLimitTemp;
    doc["hysteresis"] = config.hysteresis;
    doc["Kp"] = config.Kp;
    doc["Ki"] = config.Ki;
    doc["Kd"] = config.Kd;
    doc["setpoint"] = config.setpoint;
    doc["globalMinTempSet"] = config.globalMinTempSet;
    doc["globalMaxTempSet"] = config.globalMaxTempSet;
    JsonArray tempCurve = doc.createNestedArray("tempCurve");
    for(int i=0; i<TEMP_CURVE_POINTS; i++) {
        tempCurve.add(config.tempCurve[i]);
    }
    doc["latitude"] = config.latitude;
    doc["longitude"] = config.longitude;
    doc["DST_offset"] = config.DST_offset;
    doc["ledState"] = config.ledState;
    doc["ledBrightness"] = config.ledBrightness;
    doc["ledRed"] = config.ledRed;
    doc["ledGreen"] = config.ledGreen;
    doc["ledBlue"] = config.ledBlue;
    
    doc["logLevel"] = config.logLevel;
    doc["lastSaveTime"] = config.lastSaveTime;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AppWebServerManager::handleApplyAllSettings(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        request->send(400, "text/plain", "JSON invalide");
        return;
    }

    if (!AppWebServerManager::validateJsonConfig(doc)) {
        request->send(400, "text/plain", "Configuration invalide");
        return;
    }

    SystemConfig& config = getGlobalConfig();
    
    if (doc.containsKey("currentProfileName")) config.currentProfileName = doc["currentProfileName"].as<String>();
    if (doc.containsKey("usePWM")) config.usePWM = doc["usePWM"];
    if (doc.containsKey("weatherModeEnabled")) config.weatherModeEnabled = doc["weatherModeEnabled"];
    if (doc.containsKey("cameraEnabled")) config.cameraEnabled = doc["cameraEnabled"];
    if (doc.containsKey("cameraResolution")) config.cameraResolution = doc["cameraResolution"].as<String>();
    if (doc.containsKey("useTempCurve")) config.useTempCurve = doc["useTempCurve"];
    if (doc.containsKey("useLimitTemp")) config.useLimitTemp = doc["useLimitTemp"];
    if (doc.containsKey("hysteresis")) config.hysteresis = doc["hysteresis"];
    if (doc.containsKey("Kp")) config.Kp = doc["Kp"];
    if (doc.containsKey("Ki")) config.Ki = doc["Ki"];
    if (doc.containsKey("Kd")) config.Kd = doc["Kd"];
    if (doc.containsKey("latitude")) config.latitude = doc["latitude"];
    if (doc.containsKey("longitude")) config.longitude = doc["longitude"];
    if (doc.containsKey("DST_offset")) config.DST_offset = doc["DST_offset"];
    if (doc.containsKey("setpoint")) config.setSetpointValue((int16_t)(doc["setpoint"].as<float>() * 10));
    if (doc.containsKey("globalMinTempSet")) config.setMinTemp((int16_t)(doc["globalMinTempSet"].as<float>() * 10));
    if (doc.containsKey("globalMaxTempSet")) config.setMaxTemp((int16_t)(doc["globalMaxTempSet"].as<float>() * 10));
    if (doc.containsKey("tempCurve") && doc["tempCurve"].is<JsonArray>()) {
        JsonArray curve = doc["tempCurve"].as<JsonArray>();
        for (int i = 0; i < TEMP_CURVE_POINTS && i < curve.size(); i++) {
            config.setTempCurve(i, (int16_t)(curve[i].as<float>() * 10));
        }
    }
    if (doc.containsKey("ledState")) config.ledState = doc["ledState"];
    if (doc.containsKey("ledBrightness")) config.ledBrightness = doc["ledBrightness"];
    if (doc.containsKey("ledRed")) config.ledRed = doc["ledRed"];
    if (doc.containsKey("ledGreen")) config.ledGreen = doc["ledGreen"];
    if (doc.containsKey("ledBlue")) config.ledBlue = doc["ledBlue"];
    
    if (doc.containsKey("logLevel")) config.logLevel = doc["logLevel"];

    ConfigManager::requestSave();
    request->send(200, "text/plain", "Configuration reçue et sauvegarde demandée.");
}

void AppWebServerManager::handleSaveConfiguration(AsyncWebServerRequest *request) {
    SystemConfig& config = getGlobalConfig();
    if (ConfigManager::saveConfig(config)) {
        request->send(200, "text/plain", "Configuration sauvegardée");
    } else {
        request->send(500, "text/plain", "Erreur sauvegarde configuration");
    }
}

void AppWebServerManager::handleListProfiles(AsyncWebServerRequest *request) {
    std::vector<String> profiles = ConfigManager::listProfiles();
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();
    for (const auto& p : profiles) {
        array.add(p);
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AppWebServerManager::handleLoadProfile(AsyncWebServerRequest *request) {
    if (request->hasParam("name")) {
        String profileName = request->getParam("name")->value();
        SystemConfig tempConfig; // Utiliser une config temporaire
        if (ConfigManager::loadProfile(profileName, tempConfig)) {
            // Renvoyer la configuration chargée au format JSON
            DynamicJsonDocument doc(2048);
            doc["currentProfileName"] = tempConfig.currentProfileName;
            doc["usePWM"] = tempConfig.usePWM;
            doc["weatherModeEnabled"] = tempConfig.weatherModeEnabled;
            doc["cameraEnabled"] = tempConfig.cameraEnabled;
            doc["cameraResolution"] = tempConfig.cameraResolution;
            doc["useTempCurve"] = tempConfig.useTempCurve;
            doc["useLimitTemp"] = tempConfig.useLimitTemp;
            doc["hysteresis"] = tempConfig.hysteresis;
            doc["Kp"] = tempConfig.Kp;
            doc["Ki"] = tempConfig.Ki;
            doc["Kd"] = tempConfig.Kd;
            doc["setpoint"] = tempConfig.setpoint;
            doc["globalMinTempSet"] = tempConfig.globalMinTempSet;
            doc["globalMaxTempSet"] = tempConfig.globalMaxTempSet;
            JsonArray tempCurve = doc.createNestedArray("tempCurve");
            for(int i=0; i<TEMP_CURVE_POINTS; i++) {
                tempCurve.add(tempConfig.getTempCurve(i));
            }
            doc["latitude"] = tempConfig.latitude;
            doc["longitude"] = tempConfig.longitude;
            doc["DST_offset"] = tempConfig.DST_offset;
            doc["ledState"] = tempConfig.ledState;
            doc["ledBrightness"] = tempConfig.ledBrightness;
            doc["ledRed"] = tempConfig.ledRed;
            doc["ledGreen"] = tempConfig.ledGreen;
            doc["ledBlue"] = tempConfig.ledBlue;
            doc["logLevel"] = tempConfig.logLevel;

            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        } else {
            request->send(404, "text/plain", "Profil non trouvé");
        }
    } else {
        request->send(400, "text/plain", "Nom de profil manquant");
    }
}

void AppWebServerManager::handleSaveProfile(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        request->send(400, "text/plain", "JSON invalide");
        return;
    }
    String profileName = doc["name"];
    SystemConfig& config = getGlobalConfig();
    if (ConfigManager::saveProfile(profileName, config)) {
        request->send(200, "text/plain", "Profil sauvegardé");
    } else {
        request->send(500, "text/plain", "Erreur sauvegarde profil");
    }
}

void AppWebServerManager::handleDeleteProfile(AsyncWebServerRequest *request) {
    if (request->hasParam("name")) {
        String profileName = request->getParam("name")->value();
        if (ConfigManager::deleteProfile(profileName)) {
            request->send(200, "text/plain", "Profil supprimé");
        } else {
            request->send(500, "text/plain", "Erreur suppression profil");
        }
    } else {
        request->send(400, "text/plain", "Nom de profil manquant");
    }
}

void AppWebServerManager::handleActivateProfile(AsyncWebServerRequest *request) {
     if (request->hasParam("name")) {
        String profileName = request->getParam("name")->value();
        SystemConfig& config = getGlobalConfig();
        if (ConfigManager::loadProfile(profileName, config)) {
            ConfigManager::requestSave();
            request->send(200, "text/plain", "Profil activé et sauvegardé");
        } else {
            request->send(404, "text/plain", "Profil non trouvé");
        }
    } else {
        request->send(400, "text/plain", "Nom de profil manquant");
    }
}

void AppWebServerManager::handleGetDayData(AsyncWebServerRequest *request) {
    if (!request->hasParam("day")) {
        request->send(400, "text/plain", "Jour manquant");
        return;
    }
    int dayIndex = request->getParam("day")->value().toInt();
    SystemConfig& config = getGlobalConfig();
    float temps[24];
    if (ConfigManager::loadSeasonalData(config.currentProfileName, dayIndex, temps)) {
        DynamicJsonDocument doc(1024);
        JsonArray arr = doc.to<JsonArray>();
        for(int i=0; i<24; i++) {
            arr.add((int16_t)(temps[i] * 10)); // Convert to int16_t * 10
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(404, "text/plain", "Données non trouvées");
    }
}

void AppWebServerManager::handleSaveDayData(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        request->send(400, "text/plain", "JSON invalide");
        return;
    }
    int dayIndex = doc["day"];
    JsonArray tempArray = doc["temps"];
    float temps[24];
    for(int i=0; i<24; i++) {
        temps[i] = tempArray[i].as<float>();
    }
    SystemConfig& config = getGlobalConfig();
    // Convertir les floats en int16_t * 10 avant de sauvegarder
    int16_t tempsInt[24];
    for(int i=0; i<24; i++) {
        tempsInt[i] = (int16_t)(temps[i] * 10);
    }
    if (ConfigManager::saveSeasonalData(config.currentProfileName, dayIndex, tempsInt)) {
        request->send(200, "text/plain", "Données sauvegardées");
    } else {
        request->send(500, "text/plain", "Erreur sauvegarde");
    }
}

void AppWebServerManager::handleGetYearlyTemperatures(AsyncWebServerRequest *request) {
    SystemConfig& config = getGlobalConfig();
    const String tempPath = "/profiles/" + config.currentProfileName + "/temperature.bin";
    if (LittleFS.exists(tempPath)) {
        request->send(LittleFS, tempPath, "application/octet-stream");
    } else {
        request->send(404, "text/plain", "Fichier de données non trouvé");
    }
}

void AppWebServerManager::handleExtendMonthData(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
    request->send(501, "text/plain", "Not Implemented");
}

void AppWebServerManager::handleSmoothMonthData(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
    request->send(501, "text/plain", "Not Implemented");
}

void AppWebServerManager::handleApplyYearlyCurve(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total) {
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        request->send(400, "text/plain", "JSON invalide");
        return;
    }

    if (!doc.containsKey("tempCurve")) {
        request->send(400, "text/plain", "Paramètre 'tempCurve' manquant");
        return;
    }

    JsonArray tempCurveJson = doc["tempCurve"].as<JsonArray>();

    if (tempCurveJson.size() != 24) {
        request->send(400, "text/plain", "Courbe de température invalide");
        return;
    }

    float tempCurveFloat[24];
    for (int i = 0; i < 24; i++) {
        tempCurveFloat[i] = tempCurveJson[i].as<float>();
    }

    SystemConfig& config = getGlobalConfig();
    bool success = true;
    // Convertir les floats en int16_t * 10 avant de sauvegarder
    int16_t tempCurveInt[24];
    for (int i = 0; i < 24; i++) {
        tempCurveInt[i] = (int16_t)(tempCurveFloat[i] * 10);
    }

    for (int day = 0; day < 366; day++) {
        if (!ConfigManager::saveSeasonalData(config.currentProfileName, day, tempCurveInt)) {
            success = false;
            LOG_ERROR("WEBSERVER", "Failed to save seasonal data for day %d", day);
            break;
        }
    }

    if (success) {
        request->send(200, "text/plain", "Courbe appliquée à toute l'année");
    } else {
        request->send(500, "text/plain", "Échec de l'application de la courbe");
    }
}

void AppWebServerManager::handleStatus(AsyncWebServerRequest *request) {
    SystemConfig& config = getGlobalConfig();
    DynamicJsonDocument doc(1024); // Increased size to accommodate more fields

    // Temperature and Humidity
    doc["temperature"] = SensorManager::getCurrentTemperature();
    doc["humidity"] = SensorManager::getCurrentHumidity();

    // Heater State and Mode
    doc["heaterState"] = getHeaterOutput() > 0 ? "ON" : "OFF"; // Derived from heater_output
    doc["currentMode"] = config.usePWM ? "PID" : "Hysteresis"; // Derived from config.usePWM
    doc["consigneTemp"] = getCurrentTargetTemperature(); // Renamed from "target"

    // PID/Hysteresis parameters (for modeDetails)
    doc["Kp"] = config.Kp;
    doc["Ki"] = config.Ki;
    doc["Kd"] = config.Kd;
    doc["hysteresis"] = config.hysteresis;

    // LED State
    doc["ledState"] = config.ledState;
    doc["ledRed"] = config.ledRed;
    doc["ledGreen"] = config.ledGreen;
    doc["ledBlue"] = config.ledBlue;

    // System Status (WiFi, Time)
    doc["wifiConnected"] = WiFi.isConnected(); // Add WiFi status
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        doc["currentTime"] = mktime(&timeinfo); // Unix timestamp
    } else {
        doc["currentTime"] = 0; // Default to 0 if time not available
    }

    // Other existing fields
    doc["safety_level"] = SafetySystem::getCurrentLevel();
    doc["safety_message"] = ""; // Still empty, as before

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AppWebServerManager::handleHistory(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.to<JsonArray>();
    HistoryRecord* history = getHistory();
    int startIndex = getHistoryIndex();
    bool full = isHistoryFull();

    int count = full ? MAX_HISTORY_RECORDS : startIndex;

    for (int i = 0; i < count; i++) {
        int index = (startIndex - 1 - i + MAX_HISTORY_RECORDS) % MAX_HISTORY_RECORDS;
        if (!full && index >= startIndex) continue;

        JsonObject record = arr.createNestedObject();
        record["time"] = history[index].timestamp;
        record["temp"] = history[index].temperature;
        record["hum"] = history[index].humidity;
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void AppWebServerManager::handleSafetyStatus(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(256);
    doc["level"] = SafetySystem::getCurrentLevel();
    doc["message"] = "";
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}



void AppWebServerManager::handleDownloadProfile(AsyncWebServerRequest *request) {
    if (!request->hasParam("name")) {
        request->send(400, "text/plain", "Nom de profil manquant");
        return;
    }
    String profileName = request->getParam("name")->value();
    String path = "/profiles/" + profileName + "/general.json";

    if (LittleFS.exists(path)) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, "application/json", true);
        response->addHeader("Content-Disposition", "attachment; filename=\"" + profileName + ".json\"");
        request->send(response);
    } else {
        request->send(404, "text/plain", "Fichier de profil non trouvé.");
    }
}

void AppWebServerManager::handleDownloadSeasonalData(AsyncWebServerRequest *request) {
    if (!request->hasParam("name")) {
        request->send(400, "text/plain", "Nom de profil manquant");
        return;
    }
    String profileName = request->getParam("name")->value();
    String path = "/profiles/" + profileName + "/temperature.bin";

    if (LittleFS.exists(path)) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, "application/octet-stream", true);
        response->addHeader("Content-Disposition", "attachment; filename=\"" + profileName + ".bin\"");
        request->send(response);
    } else {
        request->send(404, "text/plain", "Fichier de données saisonnières non trouvé.");
    }
}

void AppWebServerManager::handleSetCamera(AsyncWebServerRequest *request) {
    if (request->hasParam("enabled")) {
        bool enabled = request->getParam("enabled")->value() == "1";
        getGlobalConfig().cameraEnabled = enabled;
        LOG_INFO("WEBSERVER", "Camera state set to: %s", enabled ? "ON" : "OFF");
        request->send(200, "text/plain", "OK");
    }
    else {
        request->send(400, "text/plain", "Missing 'enabled' parameter");
    }
}

bool AppWebServerManager::validateJsonConfig(const DynamicJsonDocument& doc) {
    return doc.containsKey("usePWM") && doc.containsKey("setpoint");
}
