#include "solar.h"
#include "config.h"
#include <math.h>
#include <WiFi.h>
#include <NTPClient.h>

// Variables globales externes
extern SystemConfig config;
extern SystemState state;
extern NTPClient timeClient;

// ==== CONSTANTES ====
#define PI 3.14159265359
#define RAD_TO_DEG 57.2957795131
#define DEG_TO_RAD 0.01745329252

// ==== VARIABLES PRIVÉES ====
static float sunrise_hour = 8.0f;  // Valeur par défaut
static float sunset_hour = 20.0f;  // Valeur par défaut
static uint32_t last_solar_update = 0;
static int last_day_calculated = -1;

// ==== FONCTIONS PRIVÉES ====
static float calculateSolarDeclination(int day_of_year);
static float calculateHourAngle(float latitude, float declination);
static float calculateSolarNoon(float longitude);
static int getDaysInMonth(int month, int year);
static bool isLeapYear(int year);

void updateSolarTimes() {
  // Vérification des coordonnées valides
  if (config.latitude == 0 && config.longitude == 0) {
    DEBUG_PRINTLN("Coordonnées GPS non définies - Mode fixe 8h-20h");
    sunrise_hour = DEFAULT_DAY_START_HOUR;
    sunset_hour = DEFAULT_NIGHT_START_HOUR;
    return;
  }
  
  int current_day = getCurrentDayOfYear();
  
  // Calcul seulement si jour différent ou première fois
  if (current_day != last_day_calculated) {
    DEBUG_PRINTF("Calcul heures solaires pour jour %d\n", current_day);
    
    sunrise_hour = calculateSunrise(config.latitude, config.longitude, current_day);
    sunset_hour = calculateSunset(config.latitude, config.longitude, current_day);
    
    last_day_calculated = current_day;
    last_solar_update = millis();
    
    DEBUG_PRINTF("Lever: %.2fh (%.0fh%02.0f), Coucher: %.2fh (%.0fh%02.0f)\n", 
                 sunrise_hour, floor(sunrise_hour), (sunrise_hour - floor(sunrise_hour)) * 60,
                 sunset_hour, floor(sunset_hour), (sunset_hour - floor(sunset_hour)) * 60);
  }
}

bool isDayTime() {
  float current_hour = getCurrentTimeDecimal();
  
  // Vérification si entre lever et coucher du soleil
  bool is_day = (current_hour >= sunrise_hour && current_hour < sunset_hour);
  
  DEBUG_PRINTF("Heure actuelle: %.2f, Lever: %.2f, Coucher: %.2f → %s\n",
               current_hour, sunrise_hour, sunset_hour, is_day ? "JOUR" : "NUIT");
  
  return is_day;
}

float calculateSunrise(float latitude, float longitude, int day_of_year) {
  // Calcul de la déclinaison solaire
  float declination = calculateSolarDeclination(day_of_year);
  
  // Calcul de l'angle horaire
  float hour_angle = calculateHourAngle(latitude, declination);
  
  // Calcul du midi solaire
  float solar_noon = calculateSolarNoon(longitude);
  
  // Heure du lever (midi solaire - angle horaire)
  float sunrise = solar_noon - (hour_angle / 15.0f);  // Conversion degrés en heures
  
  // Correction pour rester dans la plage 0-24h
  if (sunrise < 0) sunrise += 24;
  if (sunrise >= 24) sunrise -= 24;
  
  return sunrise;
}

float calculateSunset(float latitude, float longitude, int day_of_year) {
  // Calcul de la déclinaison solaire
  float declination = calculateSolarDeclination(day_of_year);
  
  // Calcul de l'angle horaire
  float hour_angle = calculateHourAngle(latitude, declination);
  
  // Calcul du midi solaire
  float solar_noon = calculateSolarNoon(longitude);
  
  // Heure du coucher (midi solaire + angle horaire)
  float sunset = solar_noon + (hour_angle / 15.0f);  // Conversion degrés en heures
  
  // Correction pour rester dans la plage 0-24h
  if (sunset < 0) sunset += 24;
  if (sunset >= 24) sunset -= 24;
  
  return sunset;
}

float getCurrentTimeDecimal() {
  if (state.wifi_connected && timeClient.isTimeSet()) {
    // Utilisation de l'heure NTP
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    int seconds = timeClient.getSeconds();
    
    return hours + (minutes / 60.0f) + (seconds / 3600.0f);
  } else {
    // Mode autonome - estimation basée sur millis()
    uint32_t hours_since_boot = (millis() / 3600000) % 24;
    uint32_t minutes_since_hour = ((millis() % 3600000) / 60000);
    uint32_t seconds_since_minute = ((millis() % 60000) / 1000);
    
    return hours_since_boot + (minutes_since_hour / 60.0f) + (seconds_since_minute / 3600.0f);
  }
}

int getCurrentDayOfYear() {
  if (state.wifi_connected && timeClient.isTimeSet()) {
    // Calcul basé sur la date NTP
    time_t epoch_time = timeClient.getEpochTime();
    struct tm* time_info = gmtime(&epoch_time);
    return time_info->tm_yday + 1;  // tm_yday commence à 0
  } else {
    // Estimation basée sur millis() - suppose démarrage au jour 1
    return ((millis() / 86400000) % 365) + 1;
  }
}

// ==== FONCTIONS PRIVÉES IMPLÉMENTATION ====

static float calculateSolarDeclination(int day_of_year) {
  // Formule de Cooper pour la déclinaison solaire
  float angle = (2 * PI * (day_of_year - 81)) / 365.0f;
  return 23.45f * sin(angle) * DEG_TO_RAD;
}

static float calculateHourAngle(float latitude, float declination) {
  float lat_rad = latitude * DEG_TO_RAD;
  
  // Calcul de l'angle horaire du lever/coucher
  float cos_hour_angle = -tan(lat_rad) * tan(declination);
  
  // Vérification des cas extrêmes (soleil de minuit, nuit polaire)
  if (cos_hour_angle > 1.0f) {
    // Nuit polaire - pas de lever de soleil
    return 0;
  } else if (cos_hour_angle < -1.0f) {
    // Soleil de minuit - pas de coucher de soleil
    return 12 * 15;  // 12 heures en degrés
  }
  
  float hour_angle = acos(cos_hour_angle) * RAD_TO_DEG;
  return hour_angle;
}

static float calculateSolarNoon(float longitude) {
  // Midi solaire local (en heures décimales)
  // Approximation simple : 12h - (longitude / 15)
  float solar_noon = 12.0f - (longitude / 15.0f);
  
  // Correction pour garder dans la plage 0-24h
  if (solar_noon < 0) solar_noon += 24;
  if (solar_noon >= 24) solar_noon -= 24;
  
  return solar_noon;
}