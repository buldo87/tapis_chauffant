#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==== CONFIGURATION MATÉRIEL ====
// Bus I2C
#define I2C_SDA 1
#define I2C_SCL 2

// Écran OLED SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// Capteur SHT31
#define SHT31_ADDRESS 0x44

// MOSFET tapis chauffant
#define HEATER_PIN 42

// LED Neopixel RGB
#define LED_PIN 48
#define LED_COUNT 1

// Configuration caméra ESP32S3-EYE
#define CAMERA_MODEL_ESP32S3_EYE

// ==== CONFIGURATION SYSTÈME ====
// Fréquences
#define MAIN_LOOP_INTERVAL_MS 30000  // 30 secondes
#define OLED_PAGE_INTERVAL_MS 10000  // 10 secondes par page
#define WEATHER_UPDATE_INTERVAL_MS 3600000  // 1 heure
#define PWM_FREQUENCY 1000  // 1kHz pour MOSFET
#define PWM_RESOLUTION 8    // 8 bits (0-255)
#define PWM_CHANNEL 0

// Historique
#define HISTORY_SIZE 2880  // 24h * 120 mesures/heure (30s)

// Sécurité température
#define MAX_TEMP_SECURITY 30.0f

// Transitions jour/nuit par défaut
#define DEFAULT_DAY_START_HOUR 8
#define DEFAULT_NIGHT_START_HOUR 20

// Coordonnées par défaut (Paris)
#define DEFAULT_LATITUDE 48.85f
#define DEFAULT_LONGITUDE 2.35f

// ==== STRUCTURES DE DONNÉES ====
// Modes de fonctionnement
enum HeatingMode {
  HEATING_PID = 0,
  HEATING_HYSTERESIS = 1
};

enum TargetMode {
  TARGET_DAY_NIGHT = 0,
  TARGET_WEATHER = 1
};

// Couleurs LED flash
enum FlashColor {
  FLASH_WHITE = 0,
  FLASH_RED = 1
};

// Structure historique
struct HistoryPoint {
  uint32_t timestamp;
  float temperature;
  float humidity;
  bool heating_on;
};

// Configuration système
struct SystemConfig {
  // Modes
  HeatingMode heating_mode;
  TargetMode target_mode;
  
  // Paramètres PID
  float pid_kp;
  float pid_ki;
  float pid_kd;
  
  // Paramètres hystérésis (pas d'écart, juste consigne)
  float hysteresis_target;
  
  // Paramètres jour/nuit
  float temp_day;
  float temp_night;
  
  // Coordonnées météo
  float latitude;
  float longitude;
  
  // LED flash
  uint8_t led_intensity;  // 0-255
  FlashColor led_color;
  
  // WiFi
  char wifi_ssid[32];
  char wifi_password[64];
};

// État du système
struct SystemState {
  float current_temp;
  float current_humidity;
  float target_temp;
  bool heating_on;
  bool security_mode;  // Sécurité 30°C activée
  bool wifi_connected;
  uint32_t last_weather_update;
  float weather_temp;
  bool is_day_mode;
  uint32_t sunrise_time;
  uint32_t sunset_time;
  uint32_t last_sensor_read;
  
  // PID
  float pid_error_sum;
  float pid_last_error;
  
  // Moyennes glissantes 24h
  float temp_avg_24h;
  float humidity_avg_24h;
};

// ==== PARAMÈTRES PAR DÉFAUT ====
#define DEFAULT_PID_KP 2.0f
#define DEFAULT_PID_KI 0.5f
#define DEFAULT_PID_KD 1.0f
#define DEFAULT_TEMP_DAY 25.0f
#define DEFAULT_TEMP_NIGHT 20.0f
#define DEFAULT_LED_INTENSITY 128
#define DEFAULT_FLASH_COLOR FLASH_WHITE

// ==== DEBUG ====
// Activer/désactiver les logs détaillés
#define DEBUG_ENABLED true

#if DEBUG_ENABLED
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(x, ...) Serial.printf(x, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(x, ...)
#endif

// ==== API MÉTÉO ====
#define WEATHER_API_HOST "api.open-meteo.com"
#define WEATHER_API_PORT 80
#define WEATHER_API_PATH "/v1/forecast?current_weather=true&hourly=temperature_2m,relative_humidity_2m"

// ==== WEB SERVER ====
#define WEB_SERVER_PORT 80
#define WEBSOCKET_PORT 81

#endif // CONFIG_H