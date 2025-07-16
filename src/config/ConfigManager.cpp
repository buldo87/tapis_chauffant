#include "ConfigManager.h"
#include "../utils/Logger.h"
#include <time.h>

// Variables statiques
Preferences ConfigManager::prefs;
unsigned long ConfigManager::lastSaveRequest = 0;
bool ConfigManager::savePending = false;

bool ConfigManager::initialize() {
    if (!LittleFS.begin(true)) {
        LOG_ERROR("CONFIG", "LittleFS mount failed");
        return false;
    }
    
    // Créer la structure de dossiers
    if (!LittleFS.exists("/profiles")) {
        LittleFS.mkdir("/profiles");
        LOG_INFO("CONFIG", "Dossier /profiles créé");
    }
    
    // S'assurer que le profil par défaut existe
    if (!profileExists("default")) {
        createDefaultProfile();
    }
    
    LOG_INFO("CONFIG", "ConfigManager initialisé");
    return true;
}

bool ConfigManager::loadConfig(SystemConfig& config) {
    prefs.begin("system", true);
    size_t configSize = prefs.getBytesLength("config");
    
    if (configSize == sizeof(config)) {
        prefs.getBytes("config", &config, sizeof(config));
        LOG_INFO("CONFIG", "Configuration restaurée depuis la mémoire flash");
    } else {
        LOG_WARN("CONFIG", "Aucune configuration trouvée, valeurs par défaut utilisées");
    }
    
    // Charger le nom du profil actuel
    config.currentProfileName = prefs.getString("currentProfile", "default");
    String lastSave = prefs.getString("lastSave", "jamais");
    prefs.end();
    
    // Validation
    if (!config.isValid()) {
        LOG_WARN("CONFIG", "Configuration invalide détectée, reset aux valeurs par défaut");
        config = SystemConfig(); // Reset aux valeurs par défaut
    }
    
    LOG_INFO("CONFIG", "Dernière sauvegarde: %s", lastSave.c_str());
    LOG_INFO("CONFIG", "Profil actuel: %s", config.currentProfileName.c_str());
    
    return true;
}

bool ConfigManager::saveConfig(const SystemConfig& config) {
    if (!config.isValid()) {
        LOG_ERROR("CONFIG", "Configuration invalide, sauvegarde annulée");
        return false;
    }
    
    prefs.begin("system", false);
    size_t written = prefs.putBytes("config", &config, sizeof(config));
    
    if (written == sizeof(config)) {
        // Timestamp
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
            prefs.putString("lastSave", timeStr);
        }
        
        // Profil actuel
        prefs.putString("currentProfile", config.currentProfileName);
        
        LOG_INFO("CONFIG", "Configuration sauvegardée (%zu octets)", written);
        prefs.end();
        return true;
    } else {
        LOG_ERROR("CONFIG", "Échec de la sauvegarde configuration");
        prefs.end();
        return false;
    }
}

bool ConfigManager::saveConfigIfChanged(SystemConfig& config) {
    uint32_t currentHash = calculateConfigHash(config);
    
    if (currentHash == config.configHash) {
        LOG_DEBUG("CONFIG", "Configuration inchangée, sauvegarde ignorée");
        return true;
    }
    
    config.configHash = currentHash;
    return saveConfig(config);
}

void ConfigManager::requestSave() {
    lastSaveRequest = millis();
    savePending = true;
}

void ConfigManager::processPendingSave(SystemConfig& config) {
    if (savePending && (millis() - lastSaveRequest >= SAVE_DELAY)) {
        savePending = false;
        saveConfigIfChanged(config);
    }
}

uint32_t ConfigManager::calculateConfigHash(const SystemConfig& config) {
    uint32_t hash = 0;
    const uint8_t* data = (const uint8_t*)&config;
    size_t dataSize = sizeof(config) - sizeof(config.lastSaveTime) - sizeof(config.configHash);
    
    for (size_t i = 0; i < dataSize; i++) {
        hash = hash * 31 + data[i];
    }
    return hash;
}

bool ConfigManager::createDefaultProfile() {
    const String profileDir = "/profiles/default";
    const String generalPath = profileDir + "/general.json";
    
    if (!ensureProfileDirectory("default")) {
        return false;
    }
    
    File generalFile = LittleFS.open(generalPath, "w");
    if (!generalFile) {
        LOG_ERROR("CONFIG", "Impossible de créer general.json par défaut");
        return false;
    }
    
    StaticJsonDocument<1024> doc;
    doc["name"] = "default";
    doc["timestamp"] = String(millis());
    doc["version"] = "2.0";
    doc["profileType"] = "journalier";
    
    // Paramètres par défaut
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
    doc["seasonalModeEnabled"] = false;
    doc["logLevel"] = 3; // INFO
    
    // Courbe par défaut
    JsonArray temps = doc.createNestedArray("tempCurve");
    for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
        float temp = 22.2f + (i >= 8 && i <= 20 ? 3.0f : 0.0f);
        temps.add(temp);
    }
    
    serializeJson(doc, generalFile);
    generalFile.close();
    
    // Créer les données saisonnières par défaut
    createDefaultSeasonalData("default");
    
    LOG_INFO("CONFIG", "Profil par défaut créé");
    return true;
}

bool ConfigManager::loadProfile(const String& profileName, SystemConfig& config) {
    const String generalPath = "/profiles/" + profileName + "/general.json";
    
    if (!LittleFS.exists(generalPath)) {
        LOG_ERROR("CONFIG", "Profil '%s' non trouvé", profileName.c_str());
        return false;
    }
    
    File file = LittleFS.open(generalPath, "r");
    if (!file) {
        LOG_ERROR("CONFIG", "Impossible d'ouvrir le profil '%s'", profileName.c_str());
        return false;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        LOG_ERROR("CONFIG", "JSON invalide dans le profil '%s': %s", profileName.c_str(), error.c_str());
        return false;
    }
    
    // Appliquer la configuration
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
    
    if (doc.containsKey("seasonalModeEnabled")) config.seasonalModeEnabled = doc["seasonalModeEnabled"];
    if (doc.containsKey("logLevel")) config.logLevel = doc["logLevel"];
    
    config.currentProfileName = profileName;
    LOG_INFO("CONFIG", "Profil '%s' chargé", profileName.c_str());
    
    return true;
}

bool ConfigManager::profileExists(const String& profileName) {
    const String path = "/profiles/" + profileName + "/general.json";
    return LittleFS.exists(path);
}

bool ConfigManager::ensureProfileDirectory(const String& profileName) {
    const String profileDir = "/profiles/" + profileName;
    
    if (!LittleFS.exists(profileDir)) {
        if (!LittleFS.mkdir(profileDir)) {
            LOG_ERROR("CONFIG", "Impossible de créer le dossier '%s'", profileDir.c_str());
            return false;
        }
        LOG_INFO("CONFIG", "Dossier créé: %s", profileDir.c_str());
    }
    
    return true;
}

bool ConfigManager::loadSeasonalData(const String& profileName, int dayIndex, float* temperatures) {
    const String tempPath = "/profiles/" + profileName + "/temperature.bin";
    
    if (!LittleFS.exists(tempPath)) {
        LOG_WARN("CONFIG", "Données saisonnières non trouvées pour '%s'", profileName.c_str());
        return false;
    }
    
    File file = LittleFS.open(tempPath, "r");
    if (!file) {
        return false;
    }
    
    // Position dans le fichier : jour × 24 heures × 2 bytes (int16)
    size_t offset = dayIndex * 24 * sizeof(int16_t);
    if (!file.seek(offset)) {
        file.close();
        return false;
    }
    
    // Lire 24 températures en int16 et les convertir en float
    int16_t tempInt16[24];
    size_t bytesRead = file.read((uint8_t*)tempInt16, 24 * sizeof(int16_t));
    file.close();
    
    if (bytesRead != 24 * sizeof(int16_t)) {
        return false;
    }
    
    // Conversion int16 → float
    for (int i = 0; i < 24; i++) {
        temperatures[i] = tempInt16[i] / 10.0f;
    }
    
    return true;
}

bool ConfigManager::saveSeasonalData(const String& profileName, int dayIndex, const float* temperatures) {
    const String tempPath = "/profiles/" + profileName + "/temperature.bin";
    
    File file = LittleFS.open(tempPath, "r+");
    if (!file) {
        return false;
    }
    
    size_t offset = dayIndex * 24 * sizeof(int16_t);
    if (!file.seek(offset)) {
        file.close();
        return false;
    }
    
    // Convertir float → int16 et sauvegarder
    int16_t tempInt16[24];
    for (int i = 0; i < 24; i++) {
        tempInt16[i] = (int16_t)(temperatures[i] * 10);
    }
    
    size_t bytesWritten = file.write((uint8_t*)tempInt16, 24 * sizeof(int16_t));
    file.close();
    
    return (bytesWritten == 24 * sizeof(int16_t));
}

bool ConfigManager::createDefaultSeasonalData(const String& profileName) {
    if (!ensureProfileDirectory(profileName)) {
        return false;
    }
    
    const String tempPath = "/profiles/" + profileName + "/temperature.bin";
    
    File tempFile = LittleFS.open(tempPath, "w");
    if (!tempFile) {
        LOG_ERROR("CONFIG", "Impossible de créer temperature.bin pour '%s'", profileName.c_str());
        return false;
    }
    
    float dayTemps[24];
    int16_t dayTempsInt16[24];
    
    for (int day = 0; day < 366; day++) {
        generateDefaultDayTemperatures(day, dayTemps);
        
        // Convertir en int16
        for (int hour = 0; hour < 24; hour++) {
            dayTempsInt16[hour] = (int16_t)(dayTemps[hour] * 10);
        }
        
        // Écrire ce jour
        tempFile.write((uint8_t*)dayTempsInt16, 24 * sizeof(int16_t));
    }
    
    tempFile.close();
    LOG_INFO("CONFIG", "Données saisonnières créées pour '%s'", profileName.c_str());
    return true;
}

void ConfigManager::generateDefaultDayTemperatures(int dayIndex, float* dayTemps) {
    // Variation saisonnière basée sur le jour de l'année
    float seasonalBase = 22.0f + 6.0f * sin((dayIndex / 366.0f) * 2.0f * PI - PI/2);
    
    for (int hour = 0; hour < 24; hour++) {
        float baseTemp = 22.2f + (hour >= 8 && hour <= 20 ? 3.0f : 0.0f);
        float seasonalVariation = seasonalBase - 22.0f;
        
        dayTemps[hour] = baseTemp + seasonalVariation;
        
        // Limites de sécurité
        if (dayTemps[hour] < 15.0f) dayTemps[hour] = 15.0f;
        if (dayTemps[hour] > 35.0f) dayTemps[hour] = 35.0f;
    }
}

std::vector<String> ConfigManager::listProfiles() {
    std::vector<String> profiles;
    
    File root = LittleFS.open("/profiles");
    if (!root || !root.isDirectory()) {
        return profiles;
    }
    
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            String profileName = file.name();
            if (profileExists(profileName)) {
                profiles.push_back(profileName);
            }
        }
        file = root.openNextFile();
    }
    root.close();
    
    return profiles;
}
