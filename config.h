#ifndef CONFIG_H
#define CONFIG_H

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>

// Configuration des pins
#define I2C_SDA 1
#define I2C_SCL 2
#define HEATER_PIN 42
#define LED_PIN 48
#define OLED_ADDR 0x3C
#define SHT31_ADDRESS 0x44
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Configuration PWM chauffage
#define PWM_CHANNEL 0
#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

// Configuration caméra ESP32S3_EYE
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// Constantes de sécurité
#define MAX_TEMP 30.0
#define MIN_TEMP 15.0
#define WATCHDOG_TIMEOUT 10000

// Structure de configuration
struct Config {
    // Modes de fonctionnement
    int mode_chauffage = 0;     // 0=PID, 1=Hystérésis
    int mode_consigne = 0;      // 0=Fixe, 1=Météo
    
    // Paramètres PID
    float pid_kp = 2.0;
    float pid_ki = 0.5;
    float pid_kd = 1.0;
    
    // Paramètres hystérésis
    float hyst_ecart = 0.2;
    
    // Consignes température
    float temp_jour = 28.0;
    float temp_nuit = 24.0;
    float ecart_jn = 0.0;       // Non utilisé en mode météo
    
    // Coordonnées GPS (Paris par défaut)
    float latitude = 48.85;
    float longitude = 2.35;
    
    // LED
    int led_intensity = 128;    // 0-255
    int led_color = 0;          // 0=Rouge, 1=Blanc
    
    // Streaming
    int stream_active = 1;      // 0=OFF, 1=ON
    int stream_qualite = 0;     // 0=QVGA, 1=SVGA
    
    // Debug
    bool debug_enabled = true;
};

// Variables globales
extern Config config;
extern bool wifi_connected;
extern unsigned long last_weather_update;
extern float current_temp;
extern float current_humidity;
extern float target_temp;
extern bool heating_on;
extern int current_page;
extern unsigned long last_page_change;

// Fonctions de configuration
bool loadConfig();
bool saveConfig();
void resetConfig();
void debugLog(const String& message);

#endif