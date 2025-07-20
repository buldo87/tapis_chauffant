#include "ConfigManager.h"
#include "../utils/Logger.h"
#include <time.h>

// Variables statiques
Preferences ConfigManager::prefs;
unsigned long ConfigManager::lastSaveRequest = 0;
bool ConfigManager::savePending = false;

// Define a version for the preferences data structure
// Increment this if the structure of SystemConfig changes significantly
// to force a reset of saved preferences.
const uint32_t PREFS_VERSION = 3; 

bool ConfigManager::initialize() {
    if (!LittleFS.begin(true)) {
        LOG_ERROR("CONFIG", "LittleFS mount failed");
        return false;
    }
    if (!LittleFS.exists("/profiles")) {
        LittleFS.mkdir("/profiles");
        LOG_INFO("CONFIG", "Dossier /profiles créé");
    }
    if (!profileExists("default")) {
        createDefaultProfile();
    }
    LOG_INFO("CONFIG", "ConfigManager initialisé");
    return true;
}

bool ConfigManager::loadConfig(SystemConfig& config) {
    prefs.begin("system", true); // Read-only mode

    uint32_t storedVersion = prefs.getUInt("prefsVersion", 0);

    if (storedVersion != PREFS_VERSION) {
        LOG_WARN("CONFIG", "Preferences version mismatch (Stored: %u, Expected: %u). Resetting to defaults.", storedVersion, PREFS_VERSION);
        prefs.end();
        prefs.begin("system", false); // Write mode to clear
        prefs.clear(); // Clear old preferences
        prefs.putUInt("prefsVersion", PREFS_VERSION); // Store current version
        prefs.end();
        config = SystemConfig(); // Reset config to defaults
        LOG_INFO("CONFIG", "Preferences reset and default config applied.");
        return true; // Successfully loaded (defaults)
    }

    // Load individual members
    config.usePWM = prefs.getBool("usePWM", config.usePWM);
    config.weatherModeEnabled = prefs.getBool("weatherMode", config.weatherModeEnabled);
    config.currentProfileName = prefs.getString("currentProfile", "default"); // Correctly load String
    config.cameraEnabled = prefs.getBool("cameraEnabled", config.cameraEnabled);
    config.cameraResolution = prefs.getString("cameraRes", config.cameraResolution); // Correctly load String
    config.useTempCurve = prefs.getBool("useTempCurve", config.useTempCurve);
    config.useLimitTemp = prefs.getBool("useLimitTemp", config.useLimitTemp);
    config.logLevel = prefs.getUChar("logLevel", config.logLevel);

    prefs.end();

    if (!config.isValid()) {
        LOG_WARN("CONFIG", "Configuration loaded is invalid. Resetting to defaults.");
        config = SystemConfig(); // Reset to defaults if validation fails
        // Re-save defaults to preferences
        saveConfig(config); 
    }
    
    String lastSave = prefs.getString("lastSave", "jamais");
    LOG_INFO("CONFIG", "Dernière sauvegarde: %s", lastSave.c_str());
    LOG_INFO("CONFIG", "Profil actuel: %s", config.currentProfileName.c_str());

    // Log file sizes for current profile
    String generalPath = "/profiles/" + config.currentProfileName + "/general.json";
    String tempPath = "/profiles/" + config.currentProfileName + "/temperature.bin";

    if (LittleFS.exists(generalPath)) {
        File generalFile = LittleFS.open(generalPath, "r");
        if (generalFile) {
            LOG_INFO("CONFIG", "  general.json size: %u bytes", generalFile.size());
            generalFile.close();
        }
    } else {
        LOG_WARN("CONFIG", "  general.json not found for current profile.");
    }

    if (LittleFS.exists(tempPath)) {
        File tempFile = LittleFS.open(tempPath, "r");
        if (tempFile) {
            LOG_INFO("CONFIG", "  temperature.bin size: %u bytes", tempFile.size());
            tempFile.close();
        }
    } else {
        LOG_WARN("CONFIG", "  temperature.bin not found for current profile.");
    }
    return true;
}

bool ConfigManager::saveConfig(const SystemConfig& config) {
    if (!config.isValid()) {
        LOG_ERROR("CONFIG", "Configuration invalide, sauvegarde annulée");
        return false;
    }
    prefs.begin("system", false); // Write mode

    prefs.putUInt("prefsVersion", PREFS_VERSION); // Always save current version

    // Save individual members
    prefs.putBool("usePWM", config.usePWM);
    prefs.putBool("weatherMode", config.weatherModeEnabled);
    prefs.putString("currentProfile", config.currentProfileName); // Correctly save String
    prefs.putBool("cameraEnabled", config.cameraEnabled);
    prefs.putString("cameraRes", config.cameraResolution); // Correctly save String
    prefs.putBool("useTempCurve", config.useTempCurve);
    prefs.putBool("useLimitTemp", config.useLimitTemp);
    
    prefs.putFloat("hysteresis", config.hysteresis);
    prefs.putFloat("Kp", config.Kp);
    prefs.putFloat("Ki", config.Ki);
    prefs.putFloat("Kd", config.Kd);
    
    prefs.putShort("setpoint", config.setpoint);
    prefs.putShort("minTemp", config.globalMinTempSet);
    prefs.putShort("maxTemp", config.globalMaxTempSet);
    
    // Save tempCurve array
    prefs.putBytes("tempCurve", config.tempCurve, sizeof(config.tempCurve));

    prefs.putFloat("latitude", config.latitude);
    prefs.putFloat("longitude", config.longitude);
    prefs.putInt("DST_offset", config.DST_offset);
    
    prefs.putBool("ledState", config.ledState);
    prefs.putUChar("ledBright", config.ledBrightness);
    prefs.putUChar("ledRed", config.ledRed);
    prefs.putUChar("ledGreen", config.ledGreen);
    prefs.putUChar("ledBlue", config.ledBlue);
    
    prefs.putUInt("configVersion", config.configVersion);
    // Hash is calculated and stored in config object, then saved
    prefs.putUInt("configHash", config.configHash); 
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
        prefs.putString("lastSave", timeStr); // Correctly save String
    } else {
        prefs.putString("lastSave", "unknown");
    }
    
    prefs.putUChar("logLevel", config.logLevel);
    prefs.end();
    return true;
}

uint32_t ConfigManager::calculateConfigHash(const SystemConfig& config) {
    uint32_t hash = 0;
    // Hash relevant members individually
    hash = hash * 31 + (uint32_t)config.usePWM;
    hash = hash * 31 + (uint32_t)config.weatherModeEnabled;
    hash = hash * 31 + (uint32_t)config.cameraEnabled;
    hash = hash * 31 + (uint32_t)config.useTempCurve;
    hash = hash * 31 + (uint32_t)config.useLimitTemp;
    hash = hash * 31 + (uint32_t)(config.hysteresis * 1000); // Convert float to int for hashing
    hash = hash * 31 + (uint32_t)(config.Kp * 1000);
    hash = hash * 31 + (uint32_t)(config.Ki * 1000);
    hash = hash * 31 + (uint32_t)(config.Kd * 1000);
    hash = hash * 31 + (uint32_t)config.setpoint;
    hash = hash * 31 + (uint32_t)config.globalMinTempSet;
    hash = hash * 31 + (uint32_t)config.globalMaxTempSet;

    // Hash tempCurve array
    for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
        hash = hash * 31 + (uint32_t)config.tempCurve[i];
    }

    hash = hash * 31 + (uint32_t)(config.latitude * 10000);
    hash = hash * 31 + (uint32_t)(config.longitude * 10000);
    hash = hash * 31 + (uint32_t)config.DST_offset;

    hash = hash * 31 + (uint32_t)config.ledState;
    hash = hash * 31 + (uint32_t)config.ledBrightness;
    hash = hash * 31 + (uint32_t)config.ledRed;
    hash = hash * 31 + (uint32_t)config.ledGreen;
    hash = hash * 31 + (uint32_t)config.ledBlue;

    
    hash = hash * 31 + (uint32_t)config.logLevel;

    // For String members, hash their content
    for (char c : config.currentProfileName) {
        hash = hash * 31 + (uint32_t)c;
    }
    for (char c : config.cameraResolution) {
        hash = hash * 31 + (uint32_t)c;
    }
    // lastSaveTime is dynamic, so don't include in hash for change detection
    
    return hash;
}

void ConfigManager::requestSave() {
    lastSaveRequest = millis();
    savePending = true;
}

bool ConfigManager::saveConfigIfChanged(SystemConfig& config) {
    uint32_t currentHash = calculateConfigHash(config);
    if (currentHash != config.configHash) {
        LOG_INFO("CONFIG", "Configuration changed (hash %u -> %u). Saving...", config.configHash, currentHash);
        config.configHash = currentHash; // Update hash before saving
        return saveConfig(config);
    } else {
        LOG_INFO("CONFIG", "Configuration unchanged. No save needed.");
        return true;
    }
}

void ConfigManager::processPendingSave(SystemConfig& config) {
    if (savePending && (millis() - lastSaveRequest >= SAVE_DELAY)) {
        savePending = false;
        saveConfigIfChanged(config);
    }
}

bool ConfigManager::createDefaultProfile() {
    const String profileDir = "/profiles/default";
    const String generalPath = profileDir + "/general.json";
    if (!ensureProfileDirectory("default")) return false;
    File generalFile = LittleFS.open(generalPath, "w");
    if (!generalFile) return false;
    StaticJsonDocument<1024> doc;
    doc["name"] = "default";
    doc["timestamp"] = String(millis());
    doc["version"] = "2.0";
    doc["profileType"] = "journalier";
    doc["usePWM"] = false;
    doc["weatherModeEnabled"] = false;
    doc["cameraEnabled"] = false;
    doc["cameraResolution"] = "qvga";
    doc["useTempCurve"] = false;
    doc["useLimitTemp"] = true;
    doc["hysteresis"] = 0.3;
    doc["Kp"] = 2.0;
    doc["Ki"] = 5.0;
    doc["Kd"] = 1.0;
    doc["setpoint"] = 23.0;
    doc["latitude"] = 48.85;
    doc["longitude"] = 2.35;
    doc["DST_offset"] = 2;
    doc["globalMinTempSet"] = 15.0;
    doc["globalMaxTempSet"] = 35.0;
    doc["ledState"] = false;
    doc["ledBrightness"] = 255;
    doc["ledRed"] = 255;
    doc["ledGreen"] = 255;
    doc["ledBlue"] = 255;
    
    doc["logLevel"] = 3;
    serializeJson(doc, generalFile);
    generalFile.close();
    createDefaultSeasonalData("default");
    LOG_INFO("CONFIG", "Profil par défaut créé");
    return true;
}

bool ConfigManager::saveProfile(const String& profileName, const SystemConfig& config) {
    if (!ensureProfileDirectory(profileName)) return false;
    const String generalPath = "/profiles/" + profileName + "/general.json";
    File file = LittleFS.open(generalPath, "w");
    if (!file) return false;
    StaticJsonDocument<2048> doc;
    doc["name"] = profileName;
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
    doc["setpoint"] = config.getSetpointFloat();
    doc["latitude"] = config.latitude;
    doc["longitude"] = config.longitude;
    doc["DST_offset"] = config.DST_offset;
    doc["globalMinTempSet"] = config.getMinTempFloat();
    doc["globalMaxTempSet"] = config.getMaxTempFloat();
    JsonArray curve = doc.createNestedArray("tempCurve");
    for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
        curve.add(config.getTempCurve(i));
    }
    doc["ledState"] = config.ledState;
    doc["ledBrightness"] = config.ledBrightness;
    doc["ledRed"] = config.ledRed;
    doc["ledGreen"] = config.ledGreen;
    doc["ledBlue"] = config.ledBlue;
    
    doc["logLevel"] = config.logLevel;
    serializeJson(doc, file);
    file.close();
    return true;
}

// --- Fonctions manquantes ---

bool ConfigManager::loadProfile(const String& profileName, SystemConfig& config) {
    const String generalPath = "/profiles/" + profileName + "/general.json";
    if (!LittleFS.exists(generalPath)) return false;
    File file = LittleFS.open(generalPath, "r");
    if (!file) return false;
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;
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
    if (doc.containsKey("setpoint")) config.setSetpointValue(doc["setpoint"]);
    if (doc.containsKey("latitude")) config.latitude = doc["latitude"];
    if (doc.containsKey("longitude")) config.longitude = doc["longitude"];
    if (doc.containsKey("DST_offset")) config.DST_offset = doc["DST_offset"];
    if (doc.containsKey("globalMinTempSet")) config.setMinTemp(doc["globalMinTempSet"]);
    if (doc.containsKey("globalMaxTempSet")) config.setMaxTemp(doc["globalMaxTempSet"]);
    if (doc.containsKey("tempCurve") && doc["tempCurve"].is<JsonArray>()) {
        JsonArray tempArray = doc["tempCurve"];
        for (int i = 0; i < TEMP_CURVE_POINTS && i < tempArray.size(); i++) {
            config.setTempCurve(i, tempArray[i]);
        }
    }
    if (doc.containsKey("ledState")) config.ledState = doc["ledState"];
    if (doc.containsKey("ledBrightness")) config.ledBrightness = doc["ledBrightness"];
    if (doc.containsKey("ledRed")) config.ledRed = doc["ledRed"];
    if (doc.containsKey("ledGreen")) config.ledGreen = doc["ledGreen"];
    if (doc.containsKey("ledBlue")) config.ledBlue = doc["ledBlue"];
    
    if (doc.containsKey("logLevel")) config.logLevel = doc["logLevel"];
    config.currentProfileName = profileName;
    return true;
}

bool ConfigManager::deleteProfile(const String& profileName) {
    String path = "/profiles/" + profileName;
    return LittleFS.rmdir(path);
}

bool ConfigManager::profileExists(const String& profileName) {
    const String path = "/profiles/" + profileName + "/general.json";
    return LittleFS.exists(path);
}

std::vector<String> ConfigManager::listProfiles() {
    std::vector<String> profiles;
    File root = LittleFS.open("/profiles");
    if (!root || !root.isDirectory()) return profiles;
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            String profileName = file.name();
            profileName.replace("/profiles/", "");
            if (profileExists(profileName)) {
                profiles.push_back(profileName);
            }
        }
        file = root.openNextFile();
    }
    root.close();
    return profiles;
}

bool ConfigManager::loadSeasonalData(const String& profileName, int dayIndex, float* temperatures) {
    const String tempPath = "/profiles/" + profileName + "/temperature.bin";
    if (!LittleFS.exists(tempPath)) return false;
    File file = LittleFS.open(tempPath, "r");
    if (!file) return false;
    size_t offset = dayIndex * 24 * sizeof(int16_t);
    if (!file.seek(offset)) {
        file.close();
        return false;
    }
    int16_t tempInt16[24];
    size_t bytesRead = file.read((uint8_t*)tempInt16, 24 * sizeof(int16_t));
    file.close();
    if (bytesRead != 24 * sizeof(int16_t)) return false;
    for (int i = 0; i < 24; i++) {
        temperatures[i] = tempInt16[i] / 10.0f;
    }
    return true;
}

bool ConfigManager::saveSeasonalData(const String& profileName, int dayIndex, const float* temperatures) {
    const String tempPath = "/profiles/" + profileName + "/temperature.bin";
    File file = LittleFS.open(tempPath, "r+");
    if (!file) return false;
    size_t offset = dayIndex * 24 * sizeof(int16_t);
    if (!file.seek(offset)) {
        file.close();
        return false;
    }
    int16_t tempInt16[24];
    for (int i = 0; i < 24; i++) {
        tempInt16[i] = (int16_t)(temperatures[i] * 10);
    }
    size_t bytesWritten = file.write((uint8_t*)tempInt16, 24 * sizeof(int16_t));
    file.close();
    return (bytesWritten == 24 * sizeof(int16_t));
}

bool ConfigManager::createDefaultSeasonalData(const String& profileName) {
    if (!ensureProfileDirectory(profileName)) return false;
    const String tempPath = "/profiles/" + profileName + "/temperature.bin";
    File tempFile = LittleFS.open(tempPath, "w");
    if (!tempFile) return false;
    float dayTemps[24];
    int16_t dayTempsInt16[24];
    for (int day = 0; day < 366; day++) {
        generateDefaultDayTemperatures(day, dayTemps);
        for (int hour = 0; hour < 24; hour++) {
            dayTempsInt16[hour] = (int16_t)(dayTemps[hour] * 10);
        }
        tempFile.write((uint8_t*)dayTempsInt16, 24 * sizeof(int16_t));
    }
    tempFile.close();
    return true;
}

bool ConfigManager::ensureProfileDirectory(const String& profileName) {
    const String profileDir = "/profiles/" + profileName;
    if (!LittleFS.exists(profileDir)) {
        if (!LittleFS.mkdir(profileDir)) {
            return false;
        }
    }
    return true;
}

void ConfigManager::generateDefaultDayTemperatures(int dayIndex, float* dayTemps) {
    float seasonalBase = 22.0f + 6.0f * sin((dayIndex / 366.0f) * 2.0f * PI - PI/2);
    for (int hour = 0; hour < 24; hour++) {
        float baseTemp = 22.2f + (hour >= 8 && hour <= 20 ? 3.0f : 0.0f);
        float seasonalVariation = seasonalBase - 22.0f;
        dayTemps[hour] = baseTemp + seasonalVariation;
        if (dayTemps[hour] < 15.0f) dayTemps[hour] = 15.0f;
        if (dayTemps[hour] > 35.0f) dayTemps[hour] = 35.0f;
    }
}
