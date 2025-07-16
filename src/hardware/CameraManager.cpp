#include "CameraManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Configuration des pins pour ESP32-S3 (adaptez si nécessaire)
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_D7      16
#define CAM_PIN_D6      17
#define CAM_PIN_D5      18
#define CAM_PIN_D4      12
#define CAM_PIN_D3      10
#define CAM_PIN_D2      8
#define CAM_PIN_D1      9
#define CAM_PIN_D0      11
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

// Variables statiques
bool CameraManager::initialized = false;
String CameraManager::currentResolution = "qvga";
int CameraManager::currentQuality = 12;
camera_config_t CameraManager::cameraConfig = {};

bool CameraManager::initialize(const String& resolution) {
    if (initialized) {
        Serial.println("⚠️ Caméra déjà initialisée");
        return true;
    }
    
    Serial.println("📷 Initialisation de la caméra...");
    
    configurePins();
    if (!configureSettings(resolution)) {
        return false;
    }
    
    esp_err_t err = esp_camera_init(&cameraConfig);
    if (err != ESP_OK) {
        Serial.printf("❌ Erreur d'initialisation caméra: 0x%x\n", err);
        return false;
    }
    
    // Configuration post-initialisation
    optimizeForSpeed();
    
    // Test de capture
    if (!testCapture()) {
        Serial.println("❌ Test de capture échoué");
        shutdown();
        return false;
    }
    
    initialized = true;
    currentResolution = resolution;
    
    Serial.printf("✅ Caméra initialisée en résolution %s\n", resolution.c_str());
    printCameraInfo();
    
    return true;
}

void CameraManager::shutdown() {
    if (initialized) {
        esp_camera_deinit();
        initialized = false;
        Serial.println("📷 Caméra arrêtée");
    }
}

void CameraManager::configurePins() {
    cameraConfig.pin_pwdn = CAM_PIN_PWDN;
    cameraConfig.pin_reset = CAM_PIN_RESET;
    cameraConfig.pin_xclk = CAM_PIN_XCLK;
    cameraConfig.pin_sccb_sda = CAM_PIN_SIOD;
    cameraConfig.pin_sccb_scl = CAM_PIN_SIOC;
    
    cameraConfig.pin_d7 = CAM_PIN_D7;
    cameraConfig.pin_d6 = CAM_PIN_D6;
    cameraConfig.pin_d5 = CAM_PIN_D5;
    cameraConfig.pin_d4 = CAM_PIN_D4;
    cameraConfig.pin_d3 = CAM_PIN_D3;
    cameraConfig.pin_d2 = CAM_PIN_D2;
    cameraConfig.pin_d1 = CAM_PIN_D1;
    cameraConfig.pin_d0 = CAM_PIN_D0;
    
    cameraConfig.pin_vsync = CAM_PIN_VSYNC;
    cameraConfig.pin_href = CAM_PIN_HREF;
    cameraConfig.pin_pclk = CAM_PIN_PCLK;
}

bool CameraManager::configureSettings(const String& resolution) {
    cameraConfig.ledc_channel = LEDC_CHANNEL_0;
    cameraConfig.ledc_timer = LEDC_TIMER_0;
    cameraConfig.xclk_freq_hz = 20000000; // 20MHz
    cameraConfig.pixel_format = PIXFORMAT_JPEG;
    cameraConfig.grab_mode = CAMERA_GRAB_LATEST;
    
    // Résolution adaptative
    framesize_t frameSize = stringToFramesize(resolution);
    if (frameSize == FRAMESIZE_INVALID) {
        Serial.printf("❌ Résolution invalide: %s\n", resolution.c_str());
        return false;
    }
    
    cameraConfig.frame_size = frameSize;
    
    if (frameSize > FRAMESIZE_VGA) {
        cameraConfig.fb_count = 1; // Un seul buffer pour les grandes résolutions
        cameraConfig.jpeg_quality = 15;
    } else {
        cameraConfig.fb_count = 2; // Double buffering pour les résolutions rapides
        cameraConfig.jpeg_quality = 12;
    }
    
    return true;
}

bool CameraManager::testCapture() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        return false;
    }
    esp_camera_fb_return(fb);
    return true;
}

void CameraManager::optimizeForSpeed() {
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, 0);
        s->set_hmirror(s, 0);
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_special_effect(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s, 0);
        s->set_ae_level(s, 0);
        s->set_aec_value(s, 300);
        s->set_gain_ctrl(s, 1);
        s->set_agc_gain(s, 0);
        s->set_gainceiling(s, (gainceiling_t)0);
        s->set_bpc(s, 0);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_dcw(s, 1);
        s->set_colorbar(s, 0);
    }
}

// === Handlers pour le serveur web ===

void CameraManager::handleCapture(AsyncWebServerRequest *request) {
    if (!initialized) {
        request->send(503, "text/plain", "Caméra non initialisée");
        return;
    }
    
    camera_fb_t *fb = captureFrame();
    if (!fb) {
        request->send(500, "text/plain", "Erreur capture image");
        return;
    }
    
    AsyncWebServerResponse *response = request->beginResponse(
        "image/jpeg", 
        fb->len,
        [fb](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            if (index >= fb->len) {
                releaseFrame(fb);
                return 0;
            }
            
            size_t bytesToCopy = min(maxLen, fb->len - index);
            memcpy(buffer, fb->buf + index, bytesToCopy);
            
            if (index + bytesToCopy >= fb->len) {
                releaseFrame(fb);
            }
            
            return bytesToCopy;
        }
    );
    
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
}

void CameraManager::handleMJPEGStream(AsyncWebServerRequest *request) {
    if (!initialized) {
        request->send(503, "text/plain", "Caméra non initialisée");
        return;
    }
    
    AsyncWebServerResponse *response = request->beginChunkedResponse(
        "multipart/x-mixed-replace; boundary=--frame",
        [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            static camera_fb_t *fb = nullptr;
            static size_t sent = 0;
            static bool headerSent = false;
            
            // Nouvelle frame
            if (index == 0 || (!fb && sent == 0)) {
                if (fb) releaseFrame(fb);
                
                fb = captureFrame();
                if (!fb) return 0;
                
                sent = 0;
                headerSent = false;
            }
            
            if (!fb) return 0;
            
            // Envoyer l'en-tête MJPEG
            if (!headerSent) {
                String header = "\r\n--frame\r\n";
                header += "Content-Type: image/jpeg\r\n";
                header += "Content-Length: " + String(fb->len) + "\r\n\r\n";
                
                size_t headerLen = min((size_t)header.length(), maxLen);
                memcpy(buffer, header.c_str(), headerLen);
                
                if (headerLen < header.length()) {
                    // Pas assez de place, on abandonne
                    releaseFrame(fb);
                    fb = nullptr;
                    return 0;
                }
                
                headerSent = true;
                return headerLen;
            }
            
            // Envoyer les données JPEG
            size_t toSend = min(maxLen, fb->len - sent);
            memcpy(buffer, fb->buf + sent, toSend);
            sent += toSend;
            
            // Frame terminée ?
            if (sent >= fb->len) {
                releaseFrame(fb);
                fb = nullptr;
                sent = 0;
            }
            
            return toSend;
        }
    );
    
    // En-têtes spécifiques MJPEG
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    response->addHeader("Connection", "close");
    
    request->send(response);
}

// === Capture et libération des frames ===

camera_fb_t* CameraManager::captureFrame() {
    if (!initialized) return nullptr;
    return esp_camera_fb_get();
}

void CameraManager::releaseFrame(camera_fb_t* frame) {
    if (!initialized || !frame) return;
    esp_camera_fb_return(frame);
}

// === Configuration dynamique ===

bool CameraManager::setResolution(const String& resolution) {
    if (!initialized) return false;
    
    framesize_t frameSize = stringToFramesize(resolution);
    if (frameSize == FRAMESIZE_INVALID) return false;
    
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return false;
    
    if (s->set_framesize(s, frameSize) == 0) {
        currentResolution = resolution;
        Serial.printf("✅ Résolution changée en %s\n", resolution.c_str());
        return true;
    }
    
    return false;
}

bool CameraManager::setQuality(int quality) {
    if (!initialized) return false;
    
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return false;
    
    quality = constrain(quality, 0, 63);
    if (s->set_quality(s, quality) == 0) {
        currentQuality = quality;
        Serial.printf("✅ Qualité changée à %d\n", quality);
        return true;
    }
    
    return false;
}

bool CameraManager::setFramerate(int targetFps) {
    // Cette fonction est une simplification, le framerate dépend de plusieurs facteurs
    if (targetFps > 20) return setResolution("qvga");
    if (targetFps > 10) return setResolution("vga");
    return setResolution("svga");
}

// === Utilitaires ===

framesize_t CameraManager::stringToFramesize(const String& resolution) {
    if (resolution == "qvga") return FRAMESIZE_QVGA;
    if (resolution == "vga") return FRAMESIZE_VGA;
    if (resolution == "svga") return FRAMESIZE_SVGA;
    if (resolution == "xga") return FRAMESIZE_XGA;
    if (resolution == "uxga") return FRAMESIZE_UXGA;
    return FRAMESIZE_INVALID;
}

void CameraManager::printCameraInfo() {
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        Serial.printf("Sensor: %s, PID: 0x%02x, VER: 0x%02x, MID: 0x%04x\n",
                      "OV5640", s->id.PID, s->id.VERSION, s->id.MID);
    }
}

float CameraManager::measureFramerate(int sampleCount) {
    if (!initialized) return 0.0f;
    
    Serial.printf("🎥 Mesure du framerate (%d frames)...\n", sampleCount);
    
    unsigned long start = millis();
    int frames = 0;
    
    for (int i = 0; i < sampleCount; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            frames++;
            esp_camera_fb_return(fb);
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Laisse du temps aux autres tâches
    }
    
    unsigned long duration = millis() - start;
    if (duration == 0) return 0.0f;
    
    float fps = (frames * 1000.0f) / duration;
    
    Serial.printf("📊 Résultat: %d frames en %lu ms = %.1f FPS\n", frames, duration, fps);
    return fps;
}

bool CameraManager::setEffect(int effect) {
    if (!initialized) return false;
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;
    return (s->set_special_effect(s, effect) == 0);
}

bool CameraManager::setBrightness(int brightness) {
    if (!initialized) return false;
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;
    return (s->set_brightness(s, brightness) == 0);
}

bool CameraManager::setContrast(int contrast) {
    if (!initialized) return false;
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;
    return (s->set_contrast(s, contrast) == 0);
}

bool CameraManager::setSaturation(int saturation) {
    if (!initialized) return false;
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;
    return (s->set_saturation(s, saturation) == 0);
}

bool CameraManager::setAutoWhiteBalance(bool enabled) {
    if (!initialized) return false;
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;
    return (s->set_whitebal(s, enabled) == 0);
}