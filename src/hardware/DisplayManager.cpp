#include "DisplayManager.h"
#include "../sensors/SensorManager.h"
#include "../utils/Logger.h"
#include <WiFi.h>
#include <time.h>

// Variables statiques
Adafruit_SSD1306* DisplayManager::display = nullptr;
SemaphoreHandle_t DisplayManager::i2cMutex = nullptr;
int DisplayManager::currentPage = 0;
bool DisplayManager::autoPageChange = true;
unsigned long DisplayManager::lastPageChange = 0;
unsigned long DisplayManager::pageDuration = 5000;
bool DisplayManager::forceNextUpdate = false;

bool DisplayManager::initialize(Adafruit_SSD1306* displayPtr, SemaphoreHandle_t mutex) {
    display = displayPtr;
    i2cMutex = mutex;
    
    if (!display || !i2cMutex) {
        LOG_ERROR("DISPLAY", "DisplayManager: paramètres invalides");
        return false;
    }
    
    currentPage = 0;
    lastPageChange = millis();
    
    LOG_INFO("DISPLAY", "DisplayManager initialisé");
    return true;
}

void DisplayManager::showPage(int pageIndex, const SystemConfig& config, const SafetySystem& safety, 
                             float currentTemp, float currentHum, float heaterOutput) {
    
    if (!display) return;
    
    // Gestion automatique des pages
    if (autoPageChange && (millis() - lastPageChange > pageDuration)) {
        nextPage();
    }
    
    // Préparation de l'affichage (pas encore d'I2C)
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    
    // Mode sécurité prioritaire
    if (SafetySystem::getCurrentLevel() > SAFETY_NORMAL) { // safety object removed
        showSafetyAlert(safety);
        updateSafe();
        return;
    }
    
    // Affichage normal selon la page
    switch (currentPage) {
        case 0:
            showMainPage(config, currentTemp, currentHum, heaterOutput);
            break;
        case 1:
            showStatsPage(currentTemp, currentHum);
            break;
        case 2:
            showSystemPage(config);
            break;
        case 3:
            showModesPage(config);
            break;
        case 4:
            showProfilePage(config);
            break;
        default:
            currentPage = 0;
            showMainPage(config, currentTemp, currentHum, heaterOutput);
            break;
    }
    
    updateSafe();
}

void DisplayManager::showMainPage(const SystemConfig& config, float currentTemp, float currentHum, float heaterOutput) {
    drawHeader("VIVARIUM v2.0");
    
    display->setTextSize(1);
    display->setCursor(0, 16);
    
    // Température actuelle vs consigne
    float targetTemp = config.getSetpointFloat();
    display->printf("Temp: %.1fC (%.1fC)\n", currentTemp, targetTemp);
    
    // Barre de température visuelle
    drawTemperatureBar(currentTemp * 10, config.globalMinTempSet, config.globalMaxTempSet);
    
    display->setCursor(0, 32);
    display->printf("Hum:  %.0f%%\n", currentHum);
    
    // État du chauffage
    display->setCursor(0, 44);
    if (heaterOutput > 0) {
        int percentage = map(heaterOutput, 0, 255, 0, 100);
        display->printf("Chauf: ON (%d%%)\n", percentage);
        drawProgressBar(70, 44, 50, 8, percentage);
    } else {
        display->println("Chauf: OFF");
    }
    
    // Mode et profil
    display->setCursor(0, 56);
    display->printf("%s | %s", 
                   config.usePWM ? "PID" : "ON/OFF",
                   config.currentProfileName.c_str());
}

void DisplayManager::showStatsPage(int16_t currentTemp, float currentHum) { // currentTemp is int16_t
    drawHeader("STATISTIQUES");
    
    display->setTextSize(1);
    display->setCursor(0, 16);
    
    display->printf("T Max: %.1fC\n", (float)SensorManager::getMaxTemperature() / 10.0f);
    display->printf("T Min: %.1fC\n", (float)SensorManager::getMinTemperature() / 10.0f);
    display->printf("H Max: %.0f%%\n", SensorManager::getMaxHumidity());
    display->printf("H Min: %.0f%%\n", SensorManager::getMinHumidity());
    
    display->setCursor(0, 48);
    display->printf("Capteur: %s", SensorManager::isDataValid() ? "OK" : "ERREUR");
    
    display->setCursor(0, 56);
    unsigned long uptime = millis() / 1000;
    display->printf("Uptime: %lus", uptime);
}

void DisplayManager::showSystemPage(const SystemConfig& config) {
    drawHeader("SYSTEME");
    
    display->setTextSize(1);
    display->setCursor(0, 16);
    
    display->printf("Heure: %s\n", formatTime().c_str());
    display->printf("WiFi: %s\n", getWiFiStatus().c_str());
    display->printf("Config: v%d\n", config.configVersion);
    display->printf("Memoire: %d KB\n", ESP.getFreeHeap() / 1024);
    
    display->setCursor(0, 56);
    display->printf("IP: %s", WiFi.localIP().toString().c_str());
}

void DisplayManager::showModesPage(const SystemConfig& config) {
    drawHeader("MODES ACTIFS");
    
    display->setTextSize(1);
    display->setCursor(0, 16);
    
    display->printf("PWM:     %s\n", config.usePWM ? "ON" : "OFF");
    display->printf("Meteo:   %s\n", config.weatherModeEnabled ? "ON" : "OFF");
    display->printf("Saison:  %s\n", config.seasonalModeEnabled ? "ON" : "OFF");
    display->printf("Camera:  %s\n", config.cameraEnabled ? "ON" : "OFF");
    display->printf("LogLevel: %s", logLevelToString((LogLevel)config.logLevel));
}

void DisplayManager::showProfilePage(const SystemConfig& config) {
    drawHeader("PROFIL ACTUEL");
    
    display->setTextSize(1);
    display->setCursor(0, 16);
    
    display->printf("Nom: %s\n", config.currentProfileName.c_str());
    display->printf("PID: %.1f/%.1f/%.1f\n", config.Kp, config.Ki, config.Kd);
    display->printf("Hyster: %.1fC\n", config.hysteresis);
    display->printf("Limites: %.0f-%.0fC\n", 
                   config.getMinTempFloat(), config.getMaxTempFloat());
    
    display->setCursor(0, 56);
    display->printf("Save: %s", config.lastSaveTime);
}

void DisplayManager::showSafetyAlert(const SafetySystem& safety) {
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 0);
    display->println("ALERTE!");
    display->setTextSize(1);
    display->println(safety.lastErrorMessage);
    display->display();
}

void DisplayManager::showLogo() {
    if (!display) return;
    
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(10, 10);
    display->println("VIVARIUM");
    display->setTextSize(1);
    display->setCursor(20, 35);
    display->println("Version 2.0");
    display->setCursor(15, 50);
    display->println("Initialisation...");
    
    updateSafe();
    delay(2000);
}

void DisplayManager::showError(const String& message) {
    if (!display) return;
    
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 0);
    display->println("ERREUR:");
    display->drawLine(0, 10, display->width(), 10, SSD1306_WHITE);
    display->setCursor(0, 15);
    display->println(message);
    
    updateSafe();
}

bool DisplayManager::updateSafe() {
    if (!display || !i2cMutex) return false;
    
    // Timeout court pour l'affichage
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        if (forceNextUpdate) {
            LOG_WARN("DISPLAY", "Forçage mise à jour écran malgré mutex occupé");
            forceNextUpdate = false;
        }
        return false;
    }
    
    display->display();
    xSemaphoreGive(i2cMutex);
    
    forceNextUpdate = false;
    return true;
}

void DisplayManager::nextPage() {
    currentPage = (currentPage + 1) % totalPages;
    lastPageChange = millis();
    LOG_DEBUG("DISPLAY", "Affichage page %d/%d", currentPage + 1, totalPages);
}

void DisplayManager::previousPage() {
    currentPage = (currentPage - 1 + totalPages) % totalPages;
    lastPageChange = millis();
    LOG_DEBUG("DISPLAY", "Affichage page %d/%d", currentPage + 1, totalPages);
}

void DisplayManager::drawHeader(const String& title) {
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->println(title);
    display->drawLine(0, 10, display->width(), 10, SSD1306_WHITE);
}

void DisplayManager::drawProgressBar(int x, int y, int width, int height, float percentage) {
    // Cadre
    display->drawRect(x, y, width, height, SSD1306_WHITE);
    // Remplissage
    int fillWidth = (width - 2) * percentage / 100.0f;
    if (fillWidth > 0) {
        display->fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

void DisplayManager::drawTemperatureBar(int16_t temp, int16_t min, int16_t max) {
    int barY = 24;
    int barHeight = 6;
    int barWidth = display->width() - 10;
    
    // Cadre
    display->drawRect(5, barY, barWidth, barHeight, SSD1306_WHITE);
    
    // Position de la température actuelle
    float percentage = ((float)temp / 10.0f - (float)min / 10.0f) / ((float)max / 10.0f - (float)min / 10.0f) * 100.0f; // Convert to float for calculation
    percentage = constrain(percentage, 0, 100);
    
    int fillWidth = (barWidth - 2) * percentage / 100.0f;
    if (fillWidth > 0) {
        display->fillRect(6, barY + 1, fillWidth, barHeight - 2, SSD1306_WHITE);
    }
}

String DisplayManager::formatTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "??:??:??";
    }
    
    char timeStr[9];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    return String(timeStr);
}

String DisplayManager::getWiFiStatus() {
    if (WiFi.isConnected()) {
        int rssi = WiFi.RSSI();
        if (rssi > -50) return "Excellent";
        else if (rssi > -70) return "Bon";
        else if (rssi > -85) return "Faible";
        else return "Très faible";
    }
    return "Déconnecté";
}

void DisplayManager::setBrightness(uint8_t brightness) {
    // L'OLED SSD1306 ne supporte pas le réglage de luminosité
    // Mais on peut simuler avec un contraste
    if (display && i2cMutex) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            display->ssd1306_command(SSD1306_SETCONTRAST);
            display->ssd1306_command(brightness);
            xSemaphoreGive(i2cMutex);
        }
    }
}
