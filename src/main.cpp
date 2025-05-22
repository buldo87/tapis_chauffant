#include "wifi_credentials.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Preferences.h>
#include <time.h>
#include <PID_v1.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_camera.h"
#include "logo.h"
#include "html.h"
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5

#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

// Définition des broches I2C pour l'écran SSD1306
#define I2C_SDA 1
#define I2C_SCL 2

// Configuration de l'écran SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C  // Adresse I2C typique pour SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Configuration capteur 
Adafruit_SHT31 sht31 = Adafruit_SHT31();


// === Tapis Chauffant ===
#define HEATER_PIN 42//5

AsyncWebServer server(80);
Preferences preferences;

// === Variables globales ===
float internalTemp = NAN;
float internalHum = NAN;
float externalTemp = 0.0; // à lier à getWeatherData() si besoin
float externalHum = 0.0;
float dayTemp = 28.0;
float nightTemp = 22.0;
bool heaterState = false;
float lastTemperature = 0.0;
bool cameraEnabled = false;
double setpoint, input, output;
double Kp = 2.0, Ki = 5.0, Kd = 1.0;
float hysteresis = 0.3;
const int windowSize = 1000;
float temperatureWindow[windowSize];
int windowIndex = 0;
bool windowFilled = false;
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;
const float MAX_TEMP_VARIATION = 5.0;
const float MAX_HUMIDITY_VARIATION = 10.0;
float maxTemperature = -INFINITY;
float minTemperature = INFINITY;
float maxDayTemp = dayTemp ;
float maxNightTemp = nightTemp ;
unsigned long lastToggleTime = 0;
bool manualCycleOn = false;
bool usePWM = false;  // true = PID actif, false = tout ou rien
bool weatherModeEnabled = false;
float latitude = 48.85; // Paris par défaut
float longitude = 2.35;
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 3600000; // 1h
// === Historique circulaire en RAM uniquement ===
const int MAX_HISTORY_RECORDS = 1440;

// === Affichage OLED ===
unsigned long lastDisplayChange = 0;
int displayPage = 0;
const int pageCount = 4;

// logo
int16_t positionImageAxeHorizontal = 20;     // Position de la gauche de l’image à 20 pixels du bord gauche de l’écran
int16_t positionImageAxeVertical = 0;       // Position du haut de l’image à 16 pixels du bord haut de l’écran OLED
int16_t largeurDeLimage = 64;                // Largeur de l’image à afficher : 64 pixels
int16_t hauteurDeLimage = 64;                // Hauteur de l’image à afficher : 64 pixels


struct ExternalWeather {
  float temperature;
  float humidity;
};

struct HistoryRecord {
  time_t timestamp;
  float temperature;
  float humidity;
};

PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

HistoryRecord history[MAX_HISTORY_RECORDS];
int historyIndex = 0;
bool historyFull = false;

ExternalWeather getWeatherData() {
  ExternalWeather result = {NAN, NAN};

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) +
               "&longitude=" + String(longitude, 4) +
               "&current_weather=true&hourly=relative_humidity_2m";

  http.begin(url);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("❌ Erreur API météo (%d) \n", httpCode);
    http.end();
    return result;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("❌ Erreur JSON météo : \n");
    Serial.println(error.c_str());
    return result;
  }

  result.temperature = doc["current_weather"]["temperature"] | NAN;

  String nowStr = doc["current_weather"]["time"];
  String targetHour = nowStr.substring(0, 14) + "00";

  JsonArray timeArr = doc["hourly"]["time"];
  JsonArray humArr = doc["hourly"]["relative_humidity_2m"];
  bool found = false;

  //Serial.printf("🔎 Recherche humidité pour %s \n", targetHour.c_str());
  for (size_t i = 0; i < timeArr.size(); i++) {
    if (timeArr[i].as<String>() == targetHour) {
      result.humidity = humArr[i].as<float>();
      found = true;
      break;
    }
  }

  if (!found) {
    Serial.println("❗ Aucune humidité trouvée pour l'heure demandée\n");
  }

  //Serial.printf("🌤️ Météo ext. : %.1f°C, 💧 %s%%  \n", result.temperature,
   //             isnan(result.humidity) ? "??" : String(result.humidity, 0).c_str());

  return result;
}

void addToHistory(float temperature, float humidity) {
  time_t now = time(nullptr);
  history[historyIndex] = { now, temperature, humidity };
  historyIndex = (historyIndex + 1) % MAX_HISTORY_RECORDS;
  if (historyIndex == 0) historyFull = true;
}

void handleHistoryRequest(AsyncWebServerRequest *request) {
  String json = "[";
  int total = historyFull ? MAX_HISTORY_RECORDS : historyIndex;
  bool first = true;

  for (int i = 0; i < total; i++) {
    int idx = (historyIndex + i) % MAX_HISTORY_RECORDS;
    HistoryRecord r = history[idx];

    if (r.timestamp == 0) continue; // 🚫 ignorer valeurs par défaut

    if (!first) json += ",";
    first = false;

    json += String("{\"t\":") + r.timestamp +
            ",\"temp\":" + String(r.temperature, 1) +
            ",\"hum\":" + String(r.humidity, 1) + "}";
  }

  json += "]";
  request->send(200, "application/json", json);
}

void controlHeater(float currentTemperature) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Heure non dispo → on force la consigne jour par défaut\n");
    setpoint = dayTemp;  // fallback
  } else {
    int hour = timeinfo.tm_hour;
    setpoint = (hour >= 8 && hour < 20) ? dayTemp : nightTemp;
  }
  int hour = timeinfo.tm_hour;
  float currentMaxTemp = setpoint;
  float currentMinTemp = setpoint - hysteresis;

  unsigned long now = millis();

  if (currentTemperature >= currentMaxTemp) {
    output = 0;
    manualCycleOn = false;
  } 
  else if (currentTemperature < currentMinTemp) {
    output = 255;
    manualCycleOn = false;
  } 
  else {
    if (usePWM) {
      input = currentTemperature;
      myPID.Compute();
      output = constrain(output, 0, 255);
    } else {
      // Cycle manuel : 2s ON / 1s OFF
      if (manualCycleOn && now - lastToggleTime >= 990) {
        manualCycleOn = false;
        lastToggleTime = now;
      } else if (!manualCycleOn && now - lastToggleTime >= 2990) {
        manualCycleOn = true;
        lastToggleTime = now;
      }
      output = manualCycleOn ? 255 : 0;
    }
  }

  //Serial.printf("Heure: %d, Temp: %.1f, Cible: %.1f, Sortie: %d (%d%%), Mode: %s \n",
   //             hour, currentTemperature, setpoint, (int)output, (int)((output / 255.0) * 100),
   //             usePWM ? "PWM" : "Manuel");

  analogWrite(HEATER_PIN, output);
}

void updateInternalSensor() {
  internalTemp = sht31.readTemperature();
  internalHum = sht31.readHumidity();
 // Serial.printf("🌡️ Int: %.1f°C, 💧 %.0f%%  \n", internalTemp, internalHum);
}

bool isHumidityValid(float newHumidity) {
    static float lastValidHumidity = 0.0;
    if (lastValidHumidity == 0.0) {
        lastValidHumidity = newHumidity;
        return true;
    }
    bool isValid = abs(newHumidity - lastValidHumidity) <= MAX_HUMIDITY_VARIATION;
    if (isValid) {
        lastValidHumidity = newHumidity;
    }
    return isValid;
}

bool isTemperatureValid(float newTemp) {
    if (lastTemperature == 0.0) return true;
    return abs(newTemp - lastTemperature) <= MAX_TEMP_VARIATION;
}

String readSht31Humidity() {
    float h = sht31.readHumidity();
    static float lastValidHumidity = 0.0;
    if (isnan(h)) {
        Serial.println("Échec de lecture d'humidité\n");
        if (lastValidHumidity != 0.0) {
            return String(lastValidHumidity, 1);
        }
        return "--";
    }
    if (!isHumidityValid(h)) {
        Serial.printf("Mesure d'humidité incohérente: %.1f%% (dernière: %.1f%%) \n", h, lastValidHumidity);
        return String(lastValidHumidity, 1);
    }
    lastValidHumidity = h;
    return String(h, 1);
}

String readSht31Temperature() {
    float t = sht31.readTemperature();
    if (isnan(t)) {
        Serial.println("Échec de lecture de température \n");
        if (lastTemperature != 0.0) {
            controlHeater(lastTemperature);
            return String(lastTemperature);
        }
        return "--";
    }

    if (!isTemperatureValid(t)) {
      Serial.printf("Mesure incohérente: %.1f°C (dernière: %.1f°C) \n", t, lastTemperature);
      t = lastTemperature;
    } else {
        lastTemperature = t;
        temperatureWindow[windowIndex] = t;
        windowIndex = (windowIndex + 1) % windowSize;

      if (windowIndex == 0) {
          windowFilled = true;
      }

      // Mise à jour des valeurs max et min sur les 1000 dernières mesures
      if (windowFilled) {
          maxTemperature = -INFINITY;
          minTemperature = INFINITY;
          for (int i = 0; i < windowSize; i++) {
              if (temperatureWindow[i] > maxTemperature) maxTemperature = temperatureWindow[i];
              if (temperatureWindow[i] < minTemperature) minTemperature = temperatureWindow[i];
          }
      } else {
          if (t > maxTemperature) maxTemperature = t;
          if (t < minTemperature) minTemperature = t;
      }
  }
  controlHeater(t);
  addToHistory(t, sht31.readHumidity());
  return String(t, 1);
}

String getMovingAverage() {
  float sum = 0;
  int count = windowFilled ? windowSize : windowIndex;

  if (count == 0) return "--";

  for (int i = 0; i < count; i++) {
      sum += temperatureWindow[i];
  }
  float average = sum / count;
  return String(average, 1);
}

String getHeaterState() {
    int percentage = map(output, 0, 255, 0, 100);
    return String(percentage);
}

String getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "--";
    }
    char timeString[9];
    strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
    return String(timeString);
}

String getDayNightTransitionTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "--";
    }
    int hour = timeinfo.tm_hour;
    if (hour >= 8 && hour < 20) {
        return "Jour";
    } else {
        return "Nuit";
    }
}

void handlePWMModeSetting(AsyncWebServerRequest *request) {
  if (request->hasParam("usePWM")) {
    bool oldUsePWM = usePWM;
    usePWM = request->getParam("usePWM")->value() == "1";
    Preferences prefs;
    prefs.begin("settings", false);
    preferences.putBool("usePWM", usePWM);
    prefs.end();
    Serial.printf("[SET] Mode PWM sauvegardé : %s (avant : %s)\n", 
                  usePWM ? "activé" : "désactivé", oldUsePWM ? "activé" : "désactivé");
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètre manquant");
  }
}

void handleTemperatureSetting(AsyncWebServerRequest *request) {
  bool updated = false;
  float oldDayTemp = dayTemp;
  float oldNightTemp = nightTemp;

  if (request->hasParam("dayTemp")) {
    float newDayTemp = request->getParam("dayTemp")->value().toFloat();
    if (newDayTemp >= 5.0 && newDayTemp <= 35.0) {
      dayTemp = newDayTemp;
      preferences.putFloat("dayTemp", dayTemp);
      updated = true;
    }
  }

  if (request->hasParam("nightTemp")) {
    float newNightTemp = request->getParam("nightTemp")->value().toFloat();
    if (newNightTemp >= 5.0 && newNightTemp <= 35.0) {
      nightTemp = newNightTemp;
          Preferences prefs;
    prefs.begin("settings", false);
      prefs.putFloat("nightTemp", nightTemp);
      updated = true;
      prefs.end(); 
    }
  }

  if (updated) {
    maxDayTemp = dayTemp + hysteresis;
    maxNightTemp = nightTemp + hysteresis;
    Serial.printf("[SET] Températures J/N sauvegardées : Jour = %.1f°C, Nuit = %.1f°C (avant : %.1f / %.1f)\n", 
                  dayTemp, nightTemp, oldDayTemp, oldNightTemp);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Valeurs invalides");
  }
}

void handleHysteresisSetting(AsyncWebServerRequest *request) {
  if (request->hasParam("hysteresis")) {
    float oldValue = hysteresis;
    hysteresis = request->getParam("hysteresis")->value().toFloat();

    if (hysteresis < 0.1 || hysteresis > 5.0) {
      request->send(400, "text/plain", "Valeur invalide");
      return;
    }
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putFloat("hysteresis", hysteresis);
    prefs.end(); // ✅ Fermer après
    maxDayTemp = dayTemp + hysteresis;
    maxNightTemp = nightTemp + hysteresis;

    Serial.printf("[SET] Hystérésis sauvegardée : %.1f (ancienne : %.1f)\n", hysteresis, oldValue);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètre manquant");
  }
}

void handlePIDSetting(AsyncWebServerRequest *request) {
  if (request->hasParam("Kp") && request->hasParam("Ki") && request->hasParam("Kd")) {
    float oldKp = Kp, oldKi = Ki, oldKd = Kd;

    Kp = request->getParam("Kp")->value().toFloat();
    Ki = request->getParam("Ki")->value().toFloat();
    Kd = request->getParam("Kd")->value().toFloat();

    myPID.SetTunings(Kp, Ki, Kd);
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putFloat("Kp", Kp);
    prefs.putFloat("Ki", Ki);
    prefs.putFloat("Kd", Kd);
    prefs.end();
    Serial.printf("[SET] PID sauvegardé : Kp=%.1f, Ki=%.1f, Kd=%.1f (avant : %.1f / %.1f / %.1f)\n", 
                  Kp, Ki, Kd, oldKp, oldKi, oldKd);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètres manquants");
  }
}

void renderOLEDPage(int page, int xOffset) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  //display.setCursor(xOffset, 0);
  display.setCursor(0, 0);
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int hour = timeinfo.tm_hour;
  bool isDay = hour >= 8 && hour < 20;
  bool heatingOn = (output > 0);

  switch (page) {
    case 0:
      //si mode météo weatherModeEnabled
      if (weatherModeEnabled) {
        display.println("Temp(c) Int / Ext");
        display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
        display.setCursor(0, 13);
        display.setTextSize(2);
        display.printf("%.1f %.1f\n", internalTemp, externalTemp);

        display.setCursor(0, 34);
        display.setTextSize(1);
        display.println("Hum Int / Ext");
        display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
        display.setCursor(0, 47);
        display.setTextSize(2);  
        display.printf("%.0f%%   %.0f%%", internalHum, externalHum);

      } else {
        display.println("Temp Int");
        display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
        display.setCursor(0, 13);
        display.setTextSize(2);
        display.printf("%.1fC\n", internalTemp);

        display.setCursor(0, 34);
        display.setTextSize(1);
        display.println("Hum Int");
        display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
        display.setCursor(0, 47);
        display.setTextSize(2);  
        display.printf("%.0f%%", internalHum);
      }
      break;

    case 1:
    //si différent de mode météo
    if (!weatherModeEnabled) {
      display.setTextSize(1);
      display.println("Consignes Jour/Nuit");
      display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
      display.setCursor(0, 13);
      display.setTextSize(2);
      display.printf("J %.1fC\n", dayTemp);
      display.printf("N %.1fC\n", nightTemp);
      display.setCursor(0, 47);
      display.setTextSize(1);
      display.printf(isDay ? "Jour " : "Nuit ");
      display.printf(heatingOn ? " On " : " Off ");
      display.printf(" T %.1fC\n", internalTemp);
    } else {
      display.setTextSize(1);
      display.println("Consignes meteo");
      display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
      display.setCursor(0, 13);
      display.setTextSize(2);
      display.printf("J %.1fC\n", externalTemp);
      display.setCursor(0, 47);
      display.setTextSize(1);
      display.printf(heatingOn ? "Chauf On " : "Chauf Off ");
      display.printf("%.1fC\n", internalTemp);
    }
      break;

    case 2:
      //si mode météo weatherModeEnabled
      if (weatherModeEnabled) {
        display.println("Temp Int / Ext");
        display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
        display.setCursor(0, 13);
        display.setTextSize(2);
        display.printf("%.1fC %.1fC\n", internalTemp, externalTemp);

        display.setCursor(0, 34);
        display.setTextSize(1);
        display.println("Hum Int / Ext");
        display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
        display.setCursor(0, 47);
        display.setTextSize(2);  
        display.printf("%.0f%%   %.0f%%", internalHum, externalHum);

      } else {
        display.println("Temp Int");
        display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
        display.setCursor(0, 13);
        display.setTextSize(2);
        display.printf("%.1fC\n", internalTemp);

        display.setCursor(0, 34);
        display.setTextSize(1);
        display.println("Hum Int");
        display.drawLine(0, 44, display.width(), 44, SSD1306_WHITE);
        display.setCursor(0, 47);
        display.setTextSize(2);  
        display.printf("%.0f%%", internalHum);
      }
      break;

    case 3:
      display.setTextSize(1);
      display.println("Heure actuelle");
      display.drawLine(0, 10, display.width(), 10, SSD1306_WHITE);
      display.setCursor(0, 13);
      display.setTextSize(2);
      char buf[10];
      strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
      display.println(buf);
      display.setTextSize(2);
      display.println(heatingOn ? "Chauf On" : "Chauf Off");
      display.printf("%.1fC\n", internalTemp);
      
      break;
  }

  display.display();
}

void slideToNextPage() {
  int from = 0;
  //for (int offset = 0; offset <= SCREEN_WIDTH; offset += 8) {
    display.clearDisplay();
    //renderOLEDPage(displayPage, -offset); // current slides left
    //renderOLEDPage((displayPage + 1) % pageCount, SCREEN_WIDTH - offset); // next comes in
    renderOLEDPage((displayPage + 1) % pageCount, SCREEN_WIDTH );
    //delay(15);
  //}
  displayPage = (displayPage + 1) % pageCount;
}

String processor(const String& var) {
    if (var == "USEPWM") return usePWM ? "true" : "false";
    if (var == "TEMPERATURE") return readSht31Temperature();
    if (var == "HUMIDITY") return readSht31Humidity();
    if (var == "HEATERSTATE") return getHeaterState();
    if (var == "HEATERCLASS") {
    int percentage = map(output, 0, 255, 0, 100);
    if (percentage > 60) return "heater-high";
    else if (percentage > 20) return "heater-medium";
    else return "heater-low";
}
    if (var == "DAYTEMP") return String(dayTemp, 1);
    if (var == "NIGHTTEMP") return String(nightTemp, 1);
    if (var == "CURRENTTIME") return getCurrentTime();
    if (var == "DAYNIGHTTRANSITION") return getDayNightTransitionTime();
    if (var == "MOVINGAVERAGE") return getMovingAverage();
    if (var == "HYSTERESIS") return String(hysteresis, 1);
    if (var == "KP") return String(Kp, 1);
    if (var == "KI") return String(Ki, 1);
    if (var == "KD") return String(Kd, 1);

    if (var == "LATITUDE") return String(latitude, 4);
    if (var == "LONGITUDE") return String(longitude, 4);
    if (var == "WEATHERMODE") return weatherModeEnabled ? "true" : "false";
    
    return String();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Démarrage du système de contrôle de température ===");

  // === Initialisation des préférences ===
  if (!preferences.begin("settings", false)) {
    Serial.println("❌ Échec d'initialisation des préférences !");
  } else {
    Serial.println("✅ Préférences initialisées");
  }

  // === Chargement des paramètres mémorisés ===
 bool cameraEnabled = preferences.getBool("camera", false);


  usePWM = preferences.getBool("usePWM", false);
  weatherModeEnabled = preferences.getBool("weatherMode", false);
  dayTemp = preferences.getFloat("dayTemp", 20.0);
  nightTemp = preferences.getFloat("nightTemp", 20.0);
  hysteresis = preferences.getFloat("hysteresis", 0.1);
  Kp = preferences.getFloat("Kp", 2.0);
  Ki = preferences.getFloat("Ki", 5.0);
  Kd = preferences.getFloat("Kd", 1.0);
  latitude = preferences.getFloat("latitude", 48.85);
  longitude = preferences.getFloat("longitude", 2.35);

  // === Logs des préférences ===
  Serial.println("===================================");
  Serial.println("[BOOT] Chargement des préférences");
  if (!psramFound()) {Serial.println("❌ PSRAM non détectée !");}
  Serial.println("===================================");
  Serial.printf("  usePWM: %s\n", usePWM ? "true" : "false");
  Serial.printf("  weatherModeEnabled: %s\n", weatherModeEnabled ? "true" : "false");
  Serial.printf("  Temp J/N: %.1f / %.1f\n", dayTemp, nightTemp);
  Serial.printf("  Hystérésis: %.1f\n", hysteresis);
  Serial.printf("  PID: Kp=%.1f, Ki=%.1f, Kd=%.1f\n", Kp, Ki, Kd);
  Serial.printf("  Coord GPS: %.4f, %.4f\n", latitude, longitude);
 Serial.printf("  cameraEnabled: %s\n", cameraEnabled ? "true" : "false");
  Serial.println("===================================");

  // === Initialisation WiFi ===
  Serial.print("Connexion au réseau WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connecté! IP: " + WiFi.localIP().toString());

  // === Configuration capteurs et périphériques ===
  Wire.begin(I2C_SDA, I2C_SCL);
  pinMode(HEATER_PIN, OUTPUT);
  analogWrite(HEATER_PIN, 10);
  delay(500);
  analogWrite(HEATER_PIN, 0);

  if (!sht31.begin(0x44)) {
    Serial.println("❌ Capteur SHT30 non trouvé !");
    while (true);
  }

  // === Initialisation de l'écran OLED ===
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("❌ Échec d'initialisation OLED");
    while (true);
  }
  display.clearDisplay();
  display.setCursor(0, 0);
  display.drawBitmap(positionImageAxeHorizontal, positionImageAxeVertical, logo, largeurDeLimage, hauteurDeLimage, WHITE);
  display.display();
  
  
    // Configuration de la caméra
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Initialisation avec les paramètres d'image
  //config.frame_size = FRAMESIZE_QXGA;  // ou FRAMESIZE_VGA
  config.frame_size = FRAMESIZE_VGA;
  
config.jpeg_quality = 10;               // ✅ qualité correcte
config.fb_count = 1;  

  
  // Initialisation de la caméra
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Échec d'initialisation de la caméra: 0x%x \n", err);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Erreur caméra");
    display.display();
    return;
  }
  
  Serial.println("Caméra et écran initialisés avec succès\n");
  
  
	display.clearDisplay();
	display.setCursor(0, 0);
	display.println("Wifi");
	display.println(WiFi.localIP());
	display.display();
	
  // === Initialisation PID ===
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 255);
  myPID.SetTunings(Kp, Ki, Kd);

  // === Initialisation horloge (NTP) ===
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    ExternalWeather ext = getWeatherData();
    if (!isnan(ext.temperature)) externalTemp = ext.temperature;
    if (!isnan(ext.humidity)) externalHum = ext.humidity;
    Serial.println(&timeinfo, "Date : %A, %d %B %Y - %H:%M:%S");
  } else {
    Serial.println("❌ Impossible d'obtenir l'heure");
  }

  // === Définition des routes HTTP ===
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html, processor);
  });

  server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    doc["dayTemp"] = dayTemp;
    doc["nightTemp"] = nightTemp;
    doc["hysteresis"] = hysteresis;
    doc["Kp"] = Kp;
    doc["Ki"] = Ki;
    doc["Kd"] = Kd;
    doc["weatherMode"] = weatherModeEnabled;
    doc["usePWM"] = usePWM;
    doc["latitude"] = latitude;
    doc["longitude"] = longitude;
    doc["cameraEnabled"] = preferences.getBool("camera", false);
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/setTemp", HTTP_GET, handleTemperatureSetting);
  server.on("/setHysteresis", HTTP_GET, handleHysteresisSetting);
  server.on("/setPID", HTTP_GET, handlePIDSetting);
  server.on("/setPWMMode", HTTP_GET, handlePWMModeSetting);

server.on("/setCamera", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("enabled")) {
    bool enabled = request->getParam("enabled")->value().toInt() == 1;

    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putBool("camera", enabled);
    prefs.end();

    Serial.printf("[SET] Caméra: %s\n", enabled ? "activée" : "désactivée");
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètre manquant");
  }
});



server.on("/setWeather", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("lat") && request->hasParam("lon") && request->hasParam("enabled")) {
    latitude = request->getParam("lat")->value().toFloat();
    longitude = request->getParam("lon")->value().toFloat();
    weatherModeEnabled = request->getParam("enabled")->value().toInt() == 1;

    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putFloat("latitude", latitude);
    prefs.putFloat("longitude", longitude);
    prefs.putBool("weatherMode", weatherModeEnabled);  // ✅ clé raccourcie
    bool confirm = prefs.getBool("weatherMode", false);
    prefs.end();

    Serial.printf("[SET] Météo sauvegardée: enabled=%s, lat=%.4f, lon=%.4f\n",
                  weatherModeEnabled ? "true" : "false", latitude, longitude);
    Serial.printf("[DEBUG] Relecture après écriture: weatherMode = %s\n",
                  confirm ? "true" : "false");

    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Paramètres manquants");
  }
});


  server.on("/weatherData", HTTP_GET, [](AsyncWebServerRequest *request) {
    ExternalWeather w = getWeatherData();
    if (isnan(w.temperature) || isnan(w.humidity)) {
      request->send(503, "application/json", "{\"error\":\"invalid data\"}");
    } else {
      String json = "{\"temp\":" + String(w.temperature, 1) + ",\"hum\":" + String(w.humidity, 0) + "}";
      request->send(200, "application/json", json);
    }
  });

  // === Autres endpoints ===
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", readSht31Temperature().c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", readSht31Humidity().c_str());
  });
  server.on("/heaterState", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getHeaterState().c_str());
  });
  server.on("/currentTime", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getCurrentTime().c_str());
  });
  server.on("/dayNightTransition", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getDayNightTransitionTime().c_str());
  });
  server.on("/movingAverage", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", getMovingAverage().c_str());
  });
  server.on("/maxTemperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(maxTemperature, 1));
  });
  server.on("/minTemperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(minTemperature, 1));
  });
  server.on("/history", HTTP_GET, handleHistoryRequest);

  // === Favicon & gestion 404 ===
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    request->send(500, "text/plain", "Erreur caméra");
    return;
  }

  AsyncWebServerResponse *response = request->beginResponse(
    "image/jpeg", fb->len,
    [fb](uint8_t *buffer, size_t maxLen, size_t alreadySent) -> size_t {
      if (alreadySent >= fb->len) return 0;
      size_t toCopy = min(fb->len - alreadySent, maxLen);
      memcpy(buffer, fb->buf + alreadySent, toCopy);
      if (alreadySent + toCopy >= fb->len) esp_camera_fb_return(fb);
      return toCopy;
    });

  response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
  request->send(response);
});



  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.printf("❌ Route non trouvée : %s\n", request->url().c_str());
    request->send(404, "text/plain", "Not found");
  });

  // === Lancement serveur HTTP ===
  server.begin();
  Serial.println("✅ Serveur HTTP démarré");

    Serial.println("===================================");
    Serial.println("[BOOT] Chargement des préférences");
    Serial.println("===================================");
    Serial.printf("→ Paramètres chargés:\n");
    Serial.printf("  usePWM: %s\n", usePWM ? "true" : "false");
    Serial.printf("  weatherModeEnabled: %s\n", weatherModeEnabled ? "true" : "false");
    Serial.printf("  cameraEnabled: %s\n", cameraEnabled ? "true" : "false");
    Serial.printf("  Temp J/N: %.1f / %.1f\n", dayTemp, nightTemp);
    Serial.printf("  PID: Kp=%.1f, Ki=%.1f, Kd=%.1f\n", Kp, Ki, Kd);
    Serial.printf("  Coord GPS: %.4f, %.4f\n", latitude, longitude);
    Serial.println("===================================");
}

void loop() {
  static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 10000) {
      updateInternalSensor();
      slideToNextPage();
      lastUpdate = millis();
    }
    if (weatherModeEnabled && millis() - lastWeatherUpdate > weatherInterval) {
      lastWeatherUpdate = millis();
      ExternalWeather ext = getWeatherData();
      if (!isnan(ext.temperature)) {
        externalTemp = ext.temperature;
        setpoint = externalTemp;
        Serial.printf("Setpoint météo mis à %.1f°C \n", setpoint);
      }
      if (!isnan(ext.humidity)) {
        externalHum = ext.humidity;
      }
    }

  delay(500);
}
