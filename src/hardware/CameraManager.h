#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include "../config/SystemConfig.h" // Pour l'accès à la structure de configuration
#include "esp_camera.h"             // Pour les types et fonctions de la caméra
#include <ESPAsyncWebServer.h>      // Pour les types du serveur web

// La classe CameraManager regroupe toutes les fonctionnalités liées à la caméra.
// Elle est conçue comme une classe statique (pas besoin de créer d'objet)
// pour un accès simple et direct à ses fonctions.

class CameraManager {
public:
    // Membres de données statiques
    static bool initialized;
    static String currentResolution;
    static int currentQuality;
    static camera_config_t cameraConfig;

    /**
     * @brief Initialise le matériel de la caméra avec la configuration fournie.
     * @param config Référence à la structure de configuration globale.
     * @return true si l'initialisation a réussi, false sinon.
     */
    static bool initialize(SystemConfig& config);

    /**
     * @brief Arrête la caméra et libère les ressources.
     */
    static void shutdown();

    /**
     * @brief Effectue un test de performance rapide et affiche les FPS dans la console.
     */
    static void testSpeed();

    /**
     * @brief Change la résolution de la caméra à la volée.
     * @param resolution La nouvelle résolution ("qvga", "vga", "svga").
     * @param config Référence à la configuration pour la mettre à jour.
     */
    bool setResolution(const String& resolution, SystemConfig& config);
    void setFramerate(int framerate, SystemConfig& config);

    /**
     * @brief Affiche les informations du capteur de la caméra.
     */
    static void printCameraInfo();

    /**
     * @brief Mesure le framerate actuel de la caméra.
     * @param sampleCount Nombre de frames à capturer pour la mesure.
     * @return Le framerate mesuré en FPS.
     */
    static float measureFramerate(int sampleCount);

    /**
     * @brief Définit un effet spécial sur l'image.
     * @param effect L'ID de l'effet.
     * @return true si le réglage a réussi, false sinon.
     */
    static bool setEffect(int effect);

    /**
     * @brief Règle la luminosité de l'image.
     * @param brightness La luminosité (-2 à 2).
     * @return true si le réglage a réussi, false sinon.
     */
    static bool setBrightness(int brightness);

    /**
     * @brief Règle le contraste de l'image.
     * @param contrast Le contraste (-2 à 2).
     * @return true si le réglage a réussi, false sinon.
     */
    static bool setContrast(int contrast);

    /**
     * @brief Règle la saturation de l'image.
     * @param saturation La saturation (-2 à 2).
     * @return true si le réglage a réussi, false sinon.
     */
    static bool setSaturation(int saturation);

    /**
     * @brief Active ou désactive la balance des blancs automatique.
     * @param enabled true pour activer, false pour désactiver.
     * @return true si le réglage a réussi, false sinon.
     */
    static bool setAutoWhiteBalance(bool enabled);

    // --- Fonctions de gestion pour le serveur web (Web Handlers) ---

    /**
     * @brief Gère les requêtes pour le streaming vidéo MJPEG optimisé.
     * @param request Pointeur vers l'objet de la requête web.
     */
    static void handleStream(AsyncWebServerRequest *request);

    /**
     * @brief Gère les requêtes pour capturer une seule image fixe.
     * @param request Pointeur vers l'objet de la requête web.
     */
    static void handleCapture(AsyncWebServerRequest *request);

    /**
     * @brief Gère les requêtes sur /mjpeg (redirige vers /stream).
     * @param request Pointeur vers l'objet de la requête web.
     */
    static void handleMjpeg(AsyncWebServerRequest *request);

private:
    static void configurePins();
    static bool configureSettings(const String& resolution);
    static bool testCapture();
    static void optimizeForSpeed();
    static camera_fb_t* captureFrame();
    static void releaseFrame(camera_fb_t* frame);
    static framesize_t stringToFramesize(const String& resolution);
    static void handleMJPEGStream(AsyncWebServerRequest *request);
};

#endif // CAMERA_MANAGER_H