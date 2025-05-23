#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>

// ==== FONCTIONS PUBLIQUES ====

/**
 * @brief Met à jour les données météo via l'API OpenMeteo
 * @return true si mise à jour réussie, false sinon
 */
bool updateWeatherData();

/**
 * @brief Récupère la température météo actuelle
 * @return Température en °C (0 si non disponible)
 */
float getWeatherTemperature();

/**
 * @brief Récupère l'humidité météo actuelle
 * @return Humidité en % (0 si non disponible)
 */
float getWeatherHumidity();

/**
 * @brief Vérifie si les données météo sont récentes
 * @return true si données < 2h, false sinon
 */
bool isWeatherDataFresh();

/**
 * @brief Récupère le timestamp de la dernière mise à jour météo
 * @return Timestamp en millisecondes
 */
uint32_t getLastWeatherUpdate();

/**
 * @brief Teste la connexion API météo
 * @return true si API accessible, false sinon
 */
bool testWeatherAPI();

#endif // WEATHER_H