#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// ==== FONCTIONS PUBLIQUES ====

/**
 * @brief Initialise l'écran OLED SSD1306
 */
void initDisplay();

/**
 * @brief Met à jour l'affichage OLED (rotation automatique des pages)
 */
void updateOLED();

/**
 * @brief Force l'affichage d'une page spécifique
 * @param page_num Numéro de page (0-2)
 */
void displayPage(uint8_t page_num);

/**
 * @brief Efface l'écran
 */
void clearDisplay();

/**
 * @brief Affiche un message d'erreur
 * @param error_msg Message d'erreur à afficher
 */
void displayError(const char* error_msg);

/**
 * @brief Vérifie si l'écran est initialisé
 * @return true si écran prêt, false sinon
 */
bool isDisplayReady();

#endif // DISPLAY_H