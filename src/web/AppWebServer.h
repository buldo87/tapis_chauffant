#ifndef APPWEBSERVER_H
#define APPWEBSERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class AppWebServerManager {
public:
    static void setupRoutes(AsyncWebServer& server);

private:
    // Handlers
    static void handleGetCurrentConfig(AsyncWebServerRequest *request);
    static void handleApplyAllSettings(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total);
    static void handleSaveConfiguration(AsyncWebServerRequest *request);
    static void handleListProfiles(AsyncWebServerRequest *request);
    static void handleLoadProfile(AsyncWebServerRequest *request);
    static void handleSaveProfile(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total);
    static void handleDeleteProfile(AsyncWebServerRequest *request);
    static void handleActivateProfile(AsyncWebServerRequest *request);
    static void handleGetDayData(AsyncWebServerRequest *request);
    static void handleSaveDayData(AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total);
    static void handleGetYearlyTemperatures(AsyncWebServerRequest *request);
    static void handleStatus(AsyncWebServerRequest *request);
    static void handleHistory(AsyncWebServerRequest *request);
    static void handleSafetyStatus(AsyncWebServerRequest *request);
    static void handleCapture(AsyncWebServerRequest *request);
    static void handleMJPEG(AsyncWebServerRequest *request);
    static void handleMJPEGInfo(AsyncWebServerRequest *request);

    // Validation
    static bool validateJsonConfig(const DynamicJsonDocument& doc);
};

#endif // APPWEBSERVER_H