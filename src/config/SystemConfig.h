#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <Arduino.h>

// === CONSTANTES ===
const int TEMP_CURVE_POINTS = 24;
const int MAX_HISTORY_RECORDS = 1440;

// === ÉNUMÉRATIONS ===
enum SafetyLevel {
    SAFETY_NORMAL = 0,
    SAFETY_WARNING = 1,
    SAFETY_CRITICAL = 2,
    SAFETY_EMERGENCY = 3
};

// === STRUCTURES DE DONNÉES ===
struct ExternalWeather {
    float temperature;
    float humidity;
};

struct HistoryRecord {
    time_t timestamp;
    float temperature;
    float humidity;
};



// === CONFIGURATION PRINCIPALE ===
struct SystemConfig {
    // === CONTRÔLE PRINCIPAL ===
    bool usePWM = false;
    bool weatherModeEnabled = false;
    String currentProfileName = "default";
    bool cameraEnabled = false;
    String cameraResolution = "qvga";
    bool useTempCurve = false;
    bool useLimitTemp = true;
    
    // === PID (GARDER FLOAT POUR PRÉCISION) ===
    float hysteresis = 0.3f;
    float Kp = 2.0f, Ki = 5.0f, Kd = 1.0f;
    
    // === TEMPÉRATURES (INT16 POUR COHÉRENCE - 1 décimale) ===
    int16_t setpoint = 230;                    // 23.0°C → 230
    int16_t globalMinTempSet = 150;            // 15.0°C → 150
    int16_t globalMaxTempSet = 350;            // 35.0°C → 350
    int16_t tempCurve[TEMP_CURVE_POINTS];      // Courbe 24h en int16
    
    // === GPS (GARDER FLOAT POUR PRÉCISION) ===
    float latitude = 48.85f;
    float longitude = 2.35f;
    int DST_offset = 2;
    
    // === LED/HARDWARE ===
    bool ledState = false;
    uint8_t ledBrightness = 255;
    uint8_t ledRed = 255, ledGreen = 255, ledBlue = 255;
    
    // === MÉTADONNÉES ===
    uint32_t configVersion = 2;
    uint32_t configHash = 0;
    char lastSaveTime[20] = "never";
    uint8_t logLevel = 3; // LOG_LEVEL_INFO
    
    // === CONSTRUCTEUR ===
    SystemConfig() {
        initDefaultTempCurve();
    }
    
    // === FONCTIONS UTILITAIRES TEMPERATURE ===
    inline float getSetpointFloat() const { 
        return setpoint / 10.0f; 
    }
    inline double getSetpointDouble() const { 
        return setpoint / 10.0; 
    }
    inline void setSetpointValue(float temp) { 
        setpoint = (int16_t)(temp * 10); 
    }
    
    inline float getMinTempFloat() const { 
        return globalMinTempSet / 10.0f; 
    }
    inline void setMinTemp(float temp) { 
        globalMinTempSet = (int16_t)(temp * 10); 
    }
    
    inline float getMaxTempFloat() const { 
        return globalMaxTempSet / 10.0f; 
    }
    inline void setMaxTemp(float temp) { 
        globalMaxTempSet = (int16_t)(temp * 10); 
    }
    
    inline int16_t getTempCurve(int hour) const {
        if (hour < 0 || hour >= TEMP_CURVE_POINTS) return 220; // Default to 22.0C * 10
        return tempCurve[hour];
    }
    inline void setTempCurve(int hour, float temp) {
        if (hour >= 0 && hour < TEMP_CURVE_POINTS) {
            tempCurve[hour] = (int16_t)(temp * 10);
        }
    }
    
    // === VALIDATION ===
    bool isValid() const {
        return (configVersion > 0 && 
                latitude >= -90.0f && latitude <= 90.0f &&
                longitude >= -180.0f && longitude <= 180.0f &&
                hysteresis > 0.0f && hysteresis < 10.0f);
    }
    
public:
    void initDefaultTempCurve() {
        for (int i = 0; i < TEMP_CURVE_POINTS; i++) {
            float tempFloat = 22.2f + (i >= 8 && i <= 20 ? 3.0f : 0.0f);
            tempCurve[i] = (int16_t)(tempFloat * 10);
        }
    }
};

// === CONSTANTES DE SÉCURITÉ ===
namespace SafetyConstants {
    const float TEMP_EMERGENCY_HIGH = 40.0f;
    const float TEMP_EMERGENCY_LOW = 10.0f;
    const float TEMP_WARNING_HIGH = 35.0f;
    const float TEMP_WARNING_LOW = 15.0f;
    const float HUM_EMERGENCY_HIGH = 95.0f;
    const float HUM_EMERGENCY_LOW = 10.0f;
    const unsigned long SENSOR_TIMEOUT = 30000;
    const unsigned long SAFETY_RESET_DELAY = 300000;
    const int MAX_CONSECUTIVE_FAILURES = 3;
}

// === CONSTANTES MATÉRIELLES ===
namespace HardwareConstants {
    const int HEATER_PIN = 42;
    const int APP_PIN_NEOPIXEL = 48;
    const int NUMPIXELS = 1;
    const int I2C_SDA = 1;
    const int I2C_SCL = 2;
    const int SCREEN_WIDTH = 128;
    const int SCREEN_HEIGHT = 64;
    const int OLED_ADDR = 0x3C;
}

// === MACRO DEBUG ===


#endif // SYSTEM_CONFIG_H