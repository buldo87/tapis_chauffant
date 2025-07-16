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
    /**
     * @brief Initialise le matériel de la caméra avec la configuration fournie.
     * @param config Référence à la structure de configuration globale.
     * @return true si l'initialisation a réussi, false sinon.
     */
    static bool initialize(SystemConfig& config);

    /**
     * @brief Effectue un test de performance rapide et affiche les FPS dans la console.
     */
    static void testSpeed();

    /**
     * @brief Change la résolution de la caméra à la volée.
     * @param resolution La nouvelle résolution ("qvga", "vga", "svga").
     * @param config Référence à la configuration pour la mettre à jour.
     */
    static void setResolution(const String& resolution, SystemConfig& config);

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
};

#endif // CAMERA_MANAGER_H