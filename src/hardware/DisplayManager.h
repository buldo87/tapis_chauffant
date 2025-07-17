#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "../config/SystemConfig.h"
#include "../sensors/SafetySystem.h"
#include <Adafruit_SSD1306.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// La classe DisplayManager gère l'affichage sur l'écran OLED.
// Elle est conçue comme une classe statique pour un accès centralisé.
class DisplayManager {
public:
    /**
     * @brief Initialise le gestionnaire d'affichage.
     * @param displayPtr Pointeur vers l'objet Adafruit_SSD1306.
     * @param mutex Pointeur vers le mutex pour l'accès I2C.
     * @return true si l'initialisation a réussi, false sinon.
     */
    static bool initialize(Adafruit_SSD1306* displayPtr, SemaphoreHandle_t mutex);

    /**
     * @brief Affiche une page spécifique sur l'écran.
     * @param pageIndex Index de la page à afficher.
     * @param config Référence à la configuration système.
     * @param safety Référence au système de sécurité.
     * @param currentTemp Température actuelle.
     * @param currentHum Humidité actuelle.
     * @param heaterOutput Puissance de chauffage actuelle.
     */
    static void showPage(int pageIndex, const SystemConfig& config, const SafetySystem& safety, 
                        float currentTemp, float currentHum, float heaterOutput);

    /**
     * @brief Affiche le logo de démarrage.
     */
    static void showLogo();

    /**
     * @brief Affiche un message d'erreur.
     * @param message Le message à afficher.
     */
    static void showError(const String& message);

    /**
     * @brief Affiche une alerte de sécurité.
     * @param safety Référence au système de sécurité.
     */
    static void showSafetyAlert(const SafetySystem& safety);
    
    // --- Navigation ---

    /**
     * @brief Passe à la page suivante.
     */
    static void nextPage();

    /**
     * @brief Passe à la page précédente.
     */
    static void previousPage();

    /**
     * @brief Obtient l'index de la page actuelle.
     * @return L'index de la page actuelle.
     */
    static int getCurrentPage() { return currentPage; }

    /**
     * @brief Obtient le nombre total de pages.
     * @return Le nombre total de pages.
     */
    static int getPageCount() { return totalPages; }

    /**
     * @brief Active ou désactive le changement de page automatique.
     * @param enabled true pour activer, false pour désactiver.
     */
    static void setAutoPageChange(bool enabled) { autoPageChange = enabled; }
    
    // --- Mise à jour thread-safe ---

    /**
     * @brief Met à jour l'affichage de manière thread-safe.
     * @return true si la mise à jour a réussi, false sinon.
     */
    static bool updateSafe();

    /**
     * @brief Force la prochaine mise à jour de l'affichage.
     */
    static void forceUpdate() { forceNextUpdate = true; }
    
    // --- Configuration ---

    /**
     * @brief Règle la luminosité de l'écran (contraste).
     * @param brightness Valeur de luminosité (0-255).
     */
    static void setBrightness(uint8_t brightness);

    /**
     * @brief Définit la durée d'affichage d'une page en mode automatique.
     * @param duration Durée en millisecondes.
     */
    static void setPageDuration(unsigned long duration) { pageDuration = duration; }
    
private:
    static Adafruit_SSD1306* display;
    static SemaphoreHandle_t i2cMutex;
    static int currentPage;
    static const int totalPages = 5;
    static bool autoPageChange;
    static unsigned long lastPageChange;
    static unsigned long pageDuration;
    static bool forceNextUpdate;
    
    // Pages spécifiques
    static void showMainPage(const SystemConfig& config, float currentTemp, float currentHum, float heaterOutput);
    static void showStatsPage(int16_t currentTemp, float currentHum);
    static void showSystemPage(const SystemConfig& config);
    static void showModesPage(const SystemConfig& config);
    static void showProfilePage(const SystemConfig& config);
    
    // Utilitaires d'affichage
    static void drawHeader(const String& title);
    static void drawProgressBar(int x, int y, int width, int height, float percentage);
    static void drawTemperatureBar(int16_t temp, int16_t min, int16_t max);
    static String formatTime();
    static String getWiFiStatus();
};

#endif