#ifndef SOLAR_H
#define SOLAR_H

#include <Arduino.h>

// ==== FONCTIONS PUBLIQUES ====

/**
 * @brief Met à jour les heures de lever/coucher du soleil
 * Utilise les coordonnées GPS de la configuration
 */
void updateSolarTimes();

/**
 * @brief Détermine si nous sommes en période diurne
 * @return true si jour, false si nuit
 */
bool isDayTime();

/**
 * @brief Calcule l'heure du lever du soleil
 * @param latitude Latitude en degrés
 * @param longitude Longitude en degrés
 * @param day_of_year Jour de l'année (1-365)
 * @return Heure du lever en heures décimales (ex: 7.5 = 7h30)
 */
float calculateSunrise(float latitude, float longitude, int day_of_year);

/**
 * @brief Calcule l'heure du coucher du soleil
 * @param latitude Latitude en degrés
 * @param longitude Longitude en degrés
 * @param day_of_year Jour de l'année (1-365)
 * @return Heure du coucher en heures décimales (ex: 19.25 = 19h15)
 */
float calculateSunset(float latitude, float longitude, int day_of_year);

/**
 * @brief Récupère l'heure actuelle en heures décimales
 * @return Heure actuelle (ex: 14.5 = 14h30)
 */
float getCurrentTimeDecimal();

/**
 * @brief Récupère le jour de l'année actuel
 * @return Jour de l'année (1-365/366)
 */
int getCurrentDayOfYear();

#endif // SOLAR_H