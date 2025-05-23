#include "display.h"
#include "config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Variables globales externes
extern SystemConfig config;
extern SystemState state;

// ==== VARIABLES PRIVÉES ====
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
static bool display_initialized = false;
static uint8_t current_page = 0;
static uint32_t last_page_change = 0;

// ==== FONCTIONS PRIVÉES ====
static void displayPage1_Status();
static void displayPage2_Control();
static void displayPage3_Target();
static void drawProgressBar(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t percentage);
static void drawTemperatureGraph();

void initDisplay() {
  DEBUG_PRINTLN("Initialisation écran OLED SSD1306...");
  
  // Initialisation de l'écran
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    display_initialized = true;
    DEBUG_PRINTLN("Écran OLED initialisé avec succès");
    
    // Configuration initiale
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Message de démarrage
    display.setCursor(0, 0);
    display.println("TAPIS CHAUFFANT");
    display.println("FOURMIS V1.0");
    display.println("");
    display.println("Initialisation...");
    display.display();
    
    delay(2000);  // Affichage 2 secondes
    
  } else {
    display_initialized = false;
    DEBUG_PRINTLN("ERREUR: Impossible d'initialiser l'écran OLED");
    DEBUG_PRINTLN("Vérifiez les connexions I2C et l'adresse 0x3C");
  }
}

void updateOLED() {
  if (!display_initialized) {
    return;
  }
  
  uint32_t current_time = millis();
  
  // Rotation automatique des pages toutes les 10 secondes
  if (current_time - last_page_change >= OLED_PAGE_INTERVAL_MS) {
    current_page = (current_page + 1) % 3;  // 3 pages au total
    last_page_change = current_time;
  }
  
  displayPage(current_page);
}

void displayPage(uint8_t page_num) {
  if (!display_initialized) {
    return;
  }
  
  display.clearDisplay();
  
  switch (page_num) {
    case 0:
      displayPage1_Status();
      break;
    case 1:
      displayPage2_Control();
      break;
    case 2:
      displayPage3_Target();
      break;
    default:
      displayPage1_Status();
      break;
  }
  
  display.display();
}

void clearDisplay() {
  if (!display_initialized) {
    return;
  }
  
  display.clearDisplay();
  display.display();
}

void displayError(const char* error_msg) {
  if (!display_initialized) {
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ERREUR:");
  display.println("");
  display.println(error_msg);
  display.display();
}

bool isDisplayReady() {
  return display_initialized;
}

// ==== FONCTIONS PRIVÉES IMPLÉMENTATION ====

static void displayPage1_Status() {
  // PAGE 1: État général du système
  display.setTextSize(1);
  
  // Titre
  display.setCursor(0, 0);
  display.println("=== STATUS ===");
  
  // Température actuelle
  display.setCursor(0, 12);
  display.print("Temp: ");
  display.print(state.current_temp, 1);
  display.println("C");
  
  // Humidité
  display.setCursor(0, 22);
  display.print("Hum:  ");
  display.print(state.current_humidity, 0);
  display.println("%");
  
  // État chauffage avec indicateur visuel
  display.setCursor(0, 32);
  display.print("Chauf: ");
  if (state.security_mode) {
    display.println("SECURITE");
  } else if (state.heating_on) {
    display.println("ON");
    // Barre de progression PWM en mode PID
    if (config.heating_mode == HEATING_PID) {
      uint8_t pwm_percent = (getCurrentPWMValue() * 100) / 255;
      drawProgressBar(0, 42, 100, 8, pwm_percent);
      display.setCursor(105, 42);
      display.print(pwm_percent);
      display.print("%");
    }
  } else {
    display.println("OFF");
  }
  
  // Connexion WiFi
  display.setCursor(0, 54);
  display.print("WiFi: ");
  display.println(state.wifi_connected ? "OK" : "OFF");
  
  // Indicateur de page
  display.setCursor(118, 0);
  display.println("1/3");
}

static void displayPage2_Control() {
  // PAGE 2: Mode de contrôle et consigne
  display.setTextSize(1);
  
  // Titre
  display.setCursor(0, 0);
  display.println("=== CONTROLE ===");
  
  // Mode de chauffage
  display.setCursor(0, 12);
  display.print("Mode: ");
  display.println(config.heating_mode == HEATING_PID ? "PID" : "HYSTERESIS");
  
  // Paramètres selon le mode
  if (config.heating_mode == HEATING_PID) {
    display.setCursor(0, 22);
    display.print("Kp: ");
    display.println(config.pid_kp, 1);
    
    display.setCursor(0, 32);
    display.print("Ki: ");
    display.println(config.pid_ki, 1);
    
    display.setCursor(0, 42);
    display.print("Kd: ");
    display.println(config.pid_kd, 1);
  } else {
    display.setCursor(0, 22);
    display.println("Tout ou rien");
    
    display.setCursor(0, 32);
    display.print("Seuil: ");
    display.print(state.target_temp, 1);
    display.println("C");
  }
  
  // Mode jour/nuit
  display.setCursor(0, 54);
  display.print("Mode: ");
  display.println(state.is_day_mode ? "JOUR" : "NUIT");
  
  // Indicateur de page
  display.setCursor(118, 0);
  display.println("2/3");
}

static void displayPage3_Target() {
  // PAGE 3: Consignes et météo
  display.setTextSize(1);
  
  // Titre
  display.setCursor(0, 0);
  display.println("=== CONSIGNES ===");
  
  // Mode de consigne
  display.setCursor(0, 12);
  display.print("Source: ");
  display.println(config.target_mode == TARGET_DAY_NIGHT ? "J/N" : "METEO");
  
  if (config.target_mode == TARGET_DAY_NIGHT) {
    // Mode Jour/Nuit
    display.setCursor(0, 22);
    display.print("Jour: ");
    display.print(config.temp_day, 1);
    display.println("C");
    
    display.setCursor(0, 32);
    display.print("Nuit: ");
    display.print(config.temp_night, 1);
    display.println("C");
    
  } else {
    // Mode météo
    display.setCursor(0, 22);
    display.print("Coord:");
    display.print(config.latitude, 1);
    display.print(",");
    display.println(config.longitude, 1);
    
    display.setCursor(0, 32);
    display.print("Meteo: ");
    if (state.weather_temp > 0) {
      display.print(state.weather_temp, 1);
      display.println("C");
    } else {
      display.println("N/A");
    }
  }
  
  // Consigne actuelle
  display.setCursor(0, 42);
  display.print("Actuel: ");
  display.print(state.target_temp, 1);
  display.println("C");
  
  // Moyenne 24h
  display.setCursor(0, 54);
  display.print("Moy24h: ");
  display.print(state.temp_avg_24h, 1);
  display.println("C");
  
  // Indicateur de page
  display.setCursor(118, 0);
  display.println("3/3");
}

static void drawProgressBar(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t percentage) {
  // Cadre de la barre
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  
  // Remplissage selon le pourcentage
  int16_t fill_width = (width - 2) * percentage / 100;
  if (fill_width > 0) {
    display.fillRect(x + 1, y + 1, fill_width, height - 2, SSD1306_WHITE);
  }
}