#include "heating.h"
#include "config.h"

// Variables globales externes (définies dans main.cpp)
extern SystemConfig config;
extern SystemState state;

// ==== VARIABLES PRIVÉES ====
static bool heating_initialized = false;
static bool current_heating_state = false;
static uint8_t current_pwm_value = 0;
static uint32_t last_pid_time = 0;

// Variables PID internes
static float pid_integral = 0.0f;
static float pid_last_error = 0.0f;
static bool pid_first_run = true;

void initHeating() {
  DEBUG_PRINTLN("Initialisation système chauffage...");
  
  // Configuration PWM pour MOSFET
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(HEATER_PIN, PWM_CHANNEL);
  
  // Arrêt initial du chauffage
  ledcWrite(PWM_CHANNEL, 0);
  current_pwm_value = 0;
  current_heating_state = false;
  
  // Reset paramètres PID
  resetPIDParameters();
  
  heating_initialized = true;
  DEBUG_PRINTF("Chauffage initialisé - Pin:%d, PWM:%dHz, Résolution:%d bits\n", 
               HEATER_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
}

bool updatePIDControl(float current_temp, float target_temp) {
  if (!heating_initialized) {
    DEBUG_PRINTLN("ERREUR: Système chauffage non initialisé");
    return false;
  }
  
  uint32_t current_time = millis();
  
  // Calcul du temps écoulé depuis dernière mise à jour
  uint32_t dt_ms = current_time - last_pid_time;
  if (pid_first_run || dt_ms < 1000) {  // Minimum 1 seconde entre calculs
    last_pid_time = current_time;
    pid_first_run = false;
    return current_heating_state;  // Garde l'état précédent
  }
  
  float dt = dt_ms / 1000.0f;  // Conversion en secondes
  
  // Calcul erreur
  float error = target_temp - current_temp;
  
  // Terme proportionnel
  float proportional = config.pid_kp * error;
  
  // Terme intégral (avec limitation anti-windup)
  pid_integral += error * dt;
  // Limitation intégrale pour éviter l'emballement
  float max_integral = 100.0f / config.pid_ki;  // Limite basée sur Ki
  if (pid_integral > max_integral) pid_integral = max_integral;
  if (pid_integral < -max_integral) pid_integral = -max_integral;
  float integral = config.pid_ki * pid_integral;
  
  // Terme dérivé
  float derivative = 0.0f;
  if (dt > 0) {
    derivative = config.pid_kd * (error - pid_last_error) / dt;
  }
  
  // Sortie PID
  float pid_output = proportional + integral + derivative;
  
  // Limitation sortie (0-255 pour PWM)
  if (pid_output < 0) pid_output = 0;
  if (pid_output > 255) pid_output = 255;
  
  // Application PWM
  current_pwm_value = (uint8_t)pid_output;
  ledcWrite(PWM_CHANNEL, current_pwm_value);
  
  // Détermination état ON/OFF (seuil à 10% pour éviter oscillations)
  current_heating_state = (current_pwm_value > 25);  // 10% de 255
  
  // Sauvegarde pour prochaine itération
  pid_last_error = error;
  last_pid_time = current_time;
  
  DEBUG_PRINTF("PID - Erreur:%.2f P:%.2f I:%.2f D:%.2f PWM:%d\n", 
               error, proportional, integral, derivative, current_pwm_value);
  
  return current_heating_state;
}

bool updateHysteresisControl(float current_temp, float target_temp) {
  if (!heating_initialized) {
    DEBUG_PRINTLN("ERREUR: Système chauffage non initialisé");
    return false;
  }
  
  // Hystérésis simple : ON si temp < consigne, OFF si temp >= consigne
  bool should_heat = (current_temp < target_temp);
  
  // Application de la commande
  if (should_heat != current_heating_state) {
    current_heating_state = should_heat;
    current_pwm_value = should_heat ? 255 : 0;  // Tout ou rien
    ledcWrite(PWM_CHANNEL, current_pwm_value);
    
    DEBUG_PRINTF("Hystérésis - Temp:%.1f°C Consigne:%.1f°C → %s\n", 
                 current_temp, target_temp, should_heat ? "ON" : "OFF");
  }
  
  return current_heating_state;
}

void setHeatingOutput(bool heating_on) {
  if (!heating_initialized) {
    return;
  }
  
  current_heating_state = heating_on;
  
  if (heating_on) {
    // En mode hystérésis : pleine puissance
    // En mode PID : garder la valeur PWM calculée
    if (config.heating_mode == HEATING_HYSTERESIS) {
      current_pwm_value = 255;
    }
    // En mode PID, current_pwm_value est déjà défini par updatePIDControl
  } else {
    current_pwm_value = 0;
  }
  
  ledcWrite(PWM_CHANNEL, current_pwm_value);
}

void forceHeatingOff() {
  if (!heating_initialized) {
    return;
  }
  
  current_heating_state = false;
  current_pwm_value = 0;
  ledcWrite(PWM_CHANNEL, 0);
  
  // Reset PID pour éviter les effets de bord
  resetPIDParameters();
  
  DEBUG_PRINTLN("Chauffage forcé OFF (sécurité)");
}

bool isHeatingOn() {
  return current_heating_state;
}

uint8_t getCurrentPWMValue() {
  return current_pwm_value;
}

void resetPIDParameters() {
  pid_integral = 0.0f;
  pid_last_error = 0.0f;
  pid_first_run = true;
  last_pid_time = 0;
  
  DEBUG_PRINTLN("Paramètres PID remis à zéro");
}