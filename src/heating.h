#ifndef HEATING_H
#define HEATING_H

#include <Arduino.h>
#include "config.h"

// ==== FONCTIONS PUBLIQUES ====

/**
 * @brief Initialise le système de chauffage (PWM MOSFET)
 */
void initHeating();

/**
 * @brief Met à jour le contrôle PID
 * @param current_temp Température actuelle
 * @param target_temp Température cible
 * @return true si chauffage activé, false sinon
 */
bool updatePIDControl(float current_temp, float target_temp);

/**
 * @brief Met à jour le contrôle par hystérésis
 * @param current_temp Température actuelle
 * @param target_temp Température cible (seuil)
 * @return true si chauffage activé, false sinon
 */
bool updateHysteresisControl(float current_temp, float target_temp);

/**
 * @brief Active/désactive la sortie chauffage
 * @param heating_on true pour activer, false pour désactiver
 */
void setHeatingOutput(bool heating_on);

/**
 * @brief Force l'arrêt du chauffage (sécurité)
 */
void forceHeatingOff();

/**
 * @brief Récupère l'état actuel du chauffage
 * @return true si chauffage actif, false sinon
 */
bool isHeatingOn();

/**
 * @brief Récupère la puissance PWM actuelle (0-255)
 * @return Valeur PWM
 */
uint8_t getCurrentPWMValue();

/**
 * @brief Remet à zéro les paramètres PID (intégrale, dérivée)
 */
void resetPIDParameters();

#endif // HEATING_H