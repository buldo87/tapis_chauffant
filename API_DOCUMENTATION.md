# Documentation de l'API du Vivarium Controller

Ce document décrit les endpoints de l'API RESTful exposée par le serveur web de l'appareil.

## 1. Endpoints de Configuration

Ces endpoints permettent de lire et de modifier la configuration active de l'appareil.

---

### `GET /getCurrentConfig`

Récupère l'ensemble de la configuration système actuellement chargée en mémoire.

- **Méthode :** `GET`
- **Paramètres :** Aucun.
- **Réponse Succès (200 OK) :** `application/json`
  ```json
  {
    "usePWM": true,
    "weatherModeEnabled": false,
    "currentProfileName": "default",
    "cameraEnabled": false,
    "cameraResolution": "qvga",
    "useTempCurve": true,
    "useLimitTemp": true,
    "hysteresis": 0.3,
    "Kp": 2.0,
    "Ki": 5.0,
    "Kd": 1.0,
    "latitude": 48.85,
    "longitude": 2.35,
    "DST_offset": 2,
    "setpoint": 25.5,
    "globalMinTempSet": 18.0,
    "globalMaxTempSet": 35.0,
    "tempCurve": [22.0, 22.0, ..., 26.0, 26.0],
    "ledState": false,
    "ledBrightness": 255,
    "ledRed": 255,
    "ledGreen": 255,
    "ledBlue": 255,
    "seasonalModeEnabled": false,
    "debugModeEnabled": false,
    "lastSaveTime": "15-07-2025 10:30:00"
  }
  ```

---

### `POST /applyAllSettings`

Applique une nouvelle configuration à l'appareil. Les changements sont appliqués en mémoire immédiatement et une sauvegarde en mémoire non-volatile est demandée (elle sera effectuée quelques secondes plus tard).

- **Méthode :** `POST`
- **Corps de la requête :** `application/json`
  - Le corps doit contenir un sous-ensemble des clés retournées par `GET /getCurrentConfig`.
- **Réponse Succès (200 OK) :** `text/plain` - "Configuration appliquée"
- **Réponse Erreur (400 Bad Request) :** `text/plain` - "JSON invalide" ou "Configuration invalide" si les valeurs sont hors limites.

---

### `GET /saveConfig`

Demande une sauvegarde manuelle de la configuration actuelle en mémoire non-volatile.

- **Méthode :** `GET`
- **Paramètres :** Aucun.
- **Réponse Succès (200 OK) :** `text/plain` - "Sauvegarde demandée"

## 2. Endpoints des Profils

Ces endpoints gèrent les profils de configuration sauvegardés.

---

### `GET /listProfiles`

Liste tous les profils de configuration disponibles sur le système de fichiers.

- **Méthode :** `GET`
- **Paramètres :** Aucun.
- **Réponse Succès (200 OK) :** `application/json`
  ```json
  {
    "profiles": [
      {
        "name": "default",
        "hasGeneral": true,
        "generalSize": 1024,
        "hasTempData": true,
        "tempSize": 17568
      },
      {
        "name": "ProfileDandalais",
        "hasGeneral": true,
        "generalSize": 1050,
        "hasTempData": false
      }
    ]
  }
  ```

---

### `GET /activateProfile`

Charge un profil depuis le système de fichiers et l'applique comme configuration active.

- **Méthode :** `GET`
- **Paramètres :**
  - `name` (string, requis) : Le nom du profil à activer.
- **Réponse Succès (200 OK) :** `text/plain` - "Profil activé"
- **Réponse Erreur :**
  - `400 Bad Request` : "Paramètre 'name' manquant"
  - `404 Not Found` : "Profil non trouvé"

---

### `DELETE /deleteProfile`

Supprime un profil du système de fichiers. Le profil "default" ne peut pas être supprimé.

- **Méthode :** `DELETE`
- **Paramètres :**
  - `name` (string, requis) : Le nom du profil à supprimer.
- **Réponse Succès (200 OK) :** `text/plain` - "Profil supprimé"
- **Réponse Erreur :**
  - `400 Bad Request` : Si le nom est manquant ou est "default".
  - `500 Internal Server Error` : En cas d'échec de la suppression.

---

### `POST /saveProfile`

**Non implémenté.** Cet endpoint est prévu pour sauvegarder la configuration active sous un nouveau nom de profil.

## 3. Endpoints des Données Saisonnières

Ces endpoints permettent de gérer les courbes de température sur 366 jours pour le mode saisonnier.

---

### `GET /getDayData`

Récupère les 24 températures horaires pour un jour spécifique de l'année pour le profil actuellement actif.

- **Méthode :** `GET`
- **Paramètres :**
  - `day` (int, requis) : L'index du jour dans l'année (0 à 365).
- **Réponse Succès (200 OK) :** `application/json`
  ```json
  {
    "temperatures": [22.5, 22.0, ..., 24.5, 24.0]
  }
  ```
- **Réponse Erreur :**
  - `400 Bad Request` : Si le paramètre `day` est manquant ou invalide.
  - `500 Internal Server Error` : Si les données ne peuvent pas être lues.

---

### `POST /saveDayData`

Sauvegarde les 24 températures horaires pour un jour spécifique de l'année pour le profil actif.

- **Méthode :** `POST`
- **Paramètres URL :**
  - `day` (int, requis) : L'index du jour à sauvegarder (0 à 365).
- **Corps de la requête :** `application/json` - Un tableau de 24 nombres flottants.
  ```json
  [23.0, 22.5, 22.0, 22.0, ..., 25.0, 24.5]
  ```
- **Réponse Succès (200 OK) :** `text/plain` - "Jour sauvegardé"
- **Réponse Erreur :**
  - `400 Bad Request` : Si les paramètres ou le corps JSON sont invalides.
  - `500 Internal Server Error` : En cas d'échec de la sauvegarde.

---

### `GET /getYearlyTemperatures`

Récupère la température moyenne pour chaque jour de l'année (366 jours).

- **Méthode :** `GET`
- **Paramètres :** Aucun.
- **Réponse Succès (200 OK) :** `application/json`
  ```json
  {
    "yearlyAverages": [23.1, 23.2, ..., 25.5, 25.4]
  }
  ```

## 4. Endpoints de Statut et Temps Réel

---

### `GET /status`

Fournit un aperçu complet de l'état actuel du système. C'est l'endpoint recommandé pour le monitoring.

- **Méthode :** `GET`
- **Paramètres :** Aucun.
- **Réponse Succès (200 OK) :** `application/json`
  ```json
  {
    "heaterState": 128,
    "currentMode": "PWM",
    "setpoint": 25.5,
    "temperature": 25.4,
    "humidity": 55.2,
    "sensorValid": true,
    "safetyLevel": 0,
    "emergencyShutdown": false,
    "lastUpdate": 123456789
  }
  ```

---

### `GET /history`

Récupère les données historiques de température et d'humidité.

- **Méthode :** `GET`
- **Paramètres :** Aucun.
- **Réponse Succès (200 OK) :** `application/json` - Un tableau d'enregistrements.
  ```json
  [
    {"t": 1678886400, "temp": 25.1, "hum": 56.0},
    {"t": 1678886460, "temp": 25.2, "hum": 55.8}
  ]
  ```

---

### `GET /safetyStatus`

Récupère des informations détaillées sur l'état du système de sécurité.

- **Méthode :** `GET`
- **Paramètres :** Aucun.
- **Réponse Succès (200 OK) :** `application/json`
  ```json
  {
    "level": 0,
    "levelName": "Normal",
    "emergencyShutdown": false,
    "lastError": "",
    "consecutiveFailures": 0,
    "lastSensorRead": 2,
    "lastKnownTemp": 25.4,
    "lastKnownHum": 55.2
  }
  ```

## 5. Endpoints de la Caméra

**Note :** Les endpoints ci-dessous sont définis dans le `WebServer.cpp` mais marqués comme non implémentés. La logique réelle est gérée par `CameraManager.h` et peut être directement attachée au serveur.

- **`GET /capture`** : Capture une image fixe.
- **`GET /stream`** : Démarre un flux vidéo MJPEG.
- **`GET /mjpeg`** : Alias pour `/stream`.

## 6. Endpoints de Débogage et Journalisation

---

### `GET /setLogLevel`

Change le niveau de verbosité des logs affichés sur le port série.

- **Méthode :** `GET`
- **Paramètres :**
  - `level` (int, requis) : Le niveau de log souhaité.
    - `0`: LOG_LEVEL_NONE (Aucun log)
    - `1`: LOG_LEVEL_ERROR (Erreurs seulement)
    - `2`: LOG_LEVEL_WARN (Avertissements et erreurs)
    - `3`: LOG_LEVEL_INFO (Infos, avertissements et erreurs - par défaut)
    - `4`: LOG_LEVEL_DEBUG (Tous les logs)
- **Réponse Succès (200 OK) :** `text/plain` - "Log level set to [level_name]"
- **Réponse Erreur (400 Bad Request) :** "Missing 'level' parameter" ou "Invalid log level".
