#include "weather.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Variables globales externes
extern SystemConfig config;
extern SystemState state;

// ==== VARIABLES PRIVÉES ====
static float weather_temperature = 0.0f;
static float weather_humidity = 0.0f;
static uint32_t last_weather_update = 0;
static bool weather_data_valid = false;

// ==== FONCTIONS PRIVÉES ====
static String buildWeatherURL();
static bool parseWeatherJSON(const String& json_response);

bool updateWeatherData() {
  if (!WiFi.isConnected()) {
    DEBUG_PRINTLN("Météo: Pas de connexion WiFi");
    return false;
  }
  
  DEBUG_PRINTLN("Mise à jour données météo...");
  
  HTTPClient http;
  String url = buildWeatherURL();
  
  DEBUG_PRINTF("URL météo: %s\n", url.c_str());
  
  http.begin(url);
  http.setTimeout(10000); // Timeout 10 secondes
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DEBUG_PRINTF("Réponse météo reçue (%d bytes)\n", payload.length());
    
    if (parseWeatherJSON(payload)) {
      last_weather_update = millis();
      weather_data_valid = true;
      state.last_weather_update = last_weather_update;
      state.weather_temp = weather_temperature;
      
      DEBUG_PRINTF("Météo mise à jour - T:%.1f°C H:%.1f%%\n", 
                   weather_temperature, weather_humidity);
      
      http.end();
      return true;
    } else {
      DEBUG_PRINTLN("ERREUR: Parsing JSON météo échoué");
    }
  } else {
    DEBUG_PRINTF("ERREUR HTTP météo: %d\n", httpCode);
  }
  
  http.end();
  weather_data_valid = false;
  return false;
}

float getWeatherTemperature() {
  return weather_temperature;
}

float getWeatherHumidity() {
  return weather_humidity;
}

bool isWeatherDataValid() {
  // Données valides si moins de 2 heures
  uint32_t max_age = 2 * 3600000; // 2 heures en ms
  return weather_data_valid && 
         (millis() - last_weather_update) < max_age;
}

uint32_t getLastWeatherUpdate() {
  return last_weather_update;
}

void setWeatherLocation(float lat, float lon) {
  config.latitude = lat;
  config.longitude = lon;
  DEBUG_PRINTF("Nouvelle localisation météo: %.2f, %.2f\n", lat, lon);
  
  // Invalider les données actuelles pour forcer une mise à jour
  weather_data_valid = false;
}

// ==== FONCTIONS PRIVÉES IMPLÉMENTATION ====

static String buildWeatherURL() {
  String url = "http://";
  url += WEATHER_API_HOST;
  url += WEATHER_API_PATH;
  url += "&latitude=" + String(config.latitude, 2);
  url += "&longitude=" + String(config.longitude, 2);
  url += "&timezone=Europe/Paris";
  
  return url;
}

static bool parseWeatherJSON(const String& json_response) {
  // Taille du document JSON (ajustée selon les besoins)
  const size_t capacity = JSON_OBJECT_SIZE(10) + JSON_OBJECT_SIZE(20) + 1024;
  DynamicJsonDocument doc(capacity);
  
  // Parse du JSON
  DeserializationError error = deserializeJson(doc, json_response);
  if (error) {
    DEBUG_PRINTF("ERREUR JSON: %s\n", error.c_str());
    return false;
  }
  
  // Extraction des données météo actuelles
  if (doc.containsKey("current_weather")) {
    JsonObject current = doc["current_weather"];
    
    if (current.containsKey("temperature")) {
      weather_temperature = current["temperature"].as<float>();
    } else {
      DEBUG_PRINTLN("ERREUR: Pas de température dans la réponse");
      return false;
    }
    
    // L'API current_weather ne donne pas l'humidité directement
    // On essaie de l'extraire des données horaires si disponibles
    if (doc.containsKey("hourly")) {
      JsonObject hourly = doc["hourly"];
      if (hourly.containsKey("relative_humidity_2m")) {
        JsonArray humidity_array = hourly["relative_humidity_2m"];
        if (humidity_array.size() > 0) {
          weather_humidity = humidity_array[0].as<float>(); // Première valeur
        }
      }
    }
    
    // Si pas d'humidité, on garde la dernière valeur ou 50% par défaut
    if (weather_humidity == 0.0f) {
      weather_humidity = 50.0f; // Valeur par défaut
    }
    
    // Validation des valeurs
    if (weather_temperature < -50.0f || weather_temperature > 60.0f) {
      DEBUG_PRINTF("ERREUR: Température météo aberrante: %.1f°C\n", weather_temperature);
      return false;
    }
    
    if (weather_humidity < 0.0f || weather_humidity > 100.0f) {
      DEBUG_PRINTF("ERREUR: Humidité météo aberrante: %.1f%%\n", weather_humidity);
      return false;
    }
    
    return true;
  } else {
    DEBUG_PRINTLN("ERREUR: Pas de données current_weather dans la réponse");
    return false;
  }
}