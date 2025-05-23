#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include "config.h"

// ==== FONCTIONS PUBLIQUES ====

/**
 * @brief Initialise le système de stockage
 */
void initStorage();

/**
 * @brief Charge la configuration depuis la mémoire persistante
 * @return true si chargement réussi, false si utilisation des valeurs par défaut
 */
bool loadConfiguration();

/**
 * @brief Sauvegarde la configuration en mémoire persistante
 * @return true si sauvegarde réussie, false sinon
 */
bool saveConfiguration();

/**
 * @brief Remet la configuration aux valeurs par défaut
 */
void resetConfigurationToDefaults();

/**
 * @brief Vérifie si c'est la première utilisation
 * @return true si première utilisation, false sinon
 */
bool isFirstBoot();

/**
 * @brief Sauvegarde l'historique des données (si espace disponible)
 * @return true si sauvegarde réussie, false sinon
 */
bool saveHistoryData();

/**
 * @brief Charge l'historique des données
 * @return true si chargement réussi, false sinon
 */
bool loadHistoryData();

/**
 * @brief Efface toutes les données stockées
 */
void clearAllStoredData();

/**
 * @brief Récupère la taille utilisée en mémoire
 * @return Taille en octets
 */
size_t getStorageUsage();

#endif // STORAGE_H