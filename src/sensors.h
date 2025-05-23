#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// ==== FONCTIONS PUBLIQUES ====

/**
 * @brief Initialise les capteurs (SHT31)
 */
void initSensors();

/**
 * @brief Lit les valeurs des capteurs
 * @return true si lecture réussie, false sinon
 */
bool readSensors();

/**
 * @brief Récupère la dernière température lue
 * @return Température en °C
 */
float getSensorTemperature();

/**
 * @brief Récupère la dernière humidité lue
 * @return Humidité en %
 */
float getSensorHumidity();

/**
 * @brief Vérifie si les capteurs sont fonctionnels
 * @return true si capteurs OK, false sinon
 */
bool areSensorsReady();

/**
 * @brief Récupère le timestamp de la dernière lecture
 * @return Timestamp en millisecondes
 */
uint32_t getLastReadingTime();

#endif // SENSORS_H