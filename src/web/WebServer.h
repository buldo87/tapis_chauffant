#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include "../config/SystemConfig.h"

// La classe WebServerManager gère la configuration et la gestion du serveur web.
// Elle est conçue comme une classe statique pour regrouper les handlers (gestionnaires de requêtes).
#include <ArduinoJson.h>

class WebServerManager {
public:
    /**
     * @brief Configure toutes les routes (endpoints) du serveur web.
     * @param server Référence à l'objet AsyncWebServer.
     */
    static void setupRoutes(AsyncWebServer& server);
    
private:
    // --- Handlers de configuration ---
    static void handleGetCurrentConfig(AsyncWebServerRequest *request);
    static void handleApplyAllSettings(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total);
    static void handleSaveConfiguration(AsyncWebServerRequest *request);
    
    // --- Handlers de profils ---
    static void handleListProfiles(AsyncWebServerRequest *request);
    static void handleLoadProfile(AsyncWebServerRequest *request);
    static void handleSaveProfile(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total);
    static void handleDeleteProfile(AsyncWebServerRequest *request);
    static void handleActivateProfile(AsyncWebServerRequest *request);
    
    // --- Handlers de données saisonnières ---
    static void handleGetDayData(AsyncWebServerRequest *request);
    static void handleSaveDayData(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total);
    static void handleGetYearlyTemperatures(AsyncWebServerRequest *request);
    
    // --- Handlers de données en temps réel ---
    static void handleSensorData(AsyncWebServerRequest *request);
    static void handleStatus(AsyncWebServerRequest *request);
    static void handleHistory(AsyncWebServerRequest *request);
    static void handleSafetyStatus(AsyncWebServerRequest *request);
    
    // --- Handlers caméra ---
    static void handleCapture(AsyncWebServerRequest *request);
    static void handleVideoStream(AsyncWebServerRequest *request);
    static void handleMJPEG(AsyncWebServerRequest *request);
    
    // --- Utilitaires ---
    static String createJsonResponse(const SystemConfig& config);
    static bool validateJsonConfig(const DynamicJsonDocument& doc);
};

#endif