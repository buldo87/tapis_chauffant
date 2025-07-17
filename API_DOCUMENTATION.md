# Documentation de l'API et de l'Architecture Frontend

Ce document décrit les endpoints de l'API RESTful exposée par le serveur web de l'appareil, ainsi que l'architecture du code JavaScript côté client.

## 1. Endpoints de Configuration

Ces endpoints permettent de lire et de modifier la configuration active de l'appareil.

---

### `GET /getCurrentConfig`

Récupère l'ensemble de la configuration système actuellement chargée en mémoire.

- **Méthode :** `GET`
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

Applique une nouvelle configuration à l'appareil. Les changements sont appliqués en mémoire immédiatement et une sauvegarde en mémoire non-volatile est demandée.

- **Méthode :** `POST`
- **Corps de la requête :** `application/json`
- **Réponse Succès (200 OK) :** `text/plain` - "Configuration appliquée"

---

### `GET /saveConfig`

Demande une sauvegarde manuelle de la configuration actuelle en mémoire non-volatile.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `text/plain` - "Sauvegarde demandée"

## 2. Endpoints des Profils

---

### `GET /listProfiles`

Liste tous les profils de configuration disponibles.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `application/json`
  ```json
  {
    "profiles": [
      {"name": "default", "..."},
      {"name": "ProfileDandalais", "..."}
    ]
  }
  ```

---

### `GET /activateProfile`

Charge et active un profil.

- **Méthode :** `GET`
- **Paramètres :** `name` (string, requis)
- **Réponse Succès (200 OK) :** `text/plain` - "Profil activé"

---

### `DELETE /deleteProfile`

Supprime un profil.

- **Méthode :** `DELETE`
- **Paramètres :** `name` (string, requis)
- **Réponse Succès (200 OK) :** `text/plain` - "Profil supprimé"

---

### `POST /saveProfile`

Sauvegarde la configuration actuelle (ou un objet de configuration fourni) sous un nom de profil.

- **Méthode :** `POST`
- **Corps de la requête :** `application/json` - Un objet contenant au minimum la clé `name`.
- **Réponse Succès (200 OK) :** `text/plain` - "Profil sauvegardé"

## 3. Endpoints des Données Saisonnières

---

### `GET /getDayData`

Récupère les 24 températures pour un jour donné du profil actif.

- **Méthode :** `GET`
- **Paramètres :** `day` (int, requis, 0-365)
- **Réponse Succès (200 OK) :** `application/json`

---

### `POST /saveDayData`

Sauvegarde les 24 températures pour un jour donné du profil actif.

- **Méthode :** `POST`
- **Paramètres URL :** `day` (int, requis)
- **Corps de la requête :** `application/json` - Tableau de 24 flottants.
- **Réponse Succès (200 OK) :** `text/plain` - "Jour sauvegardé"

---

### `GET /getYearlyTemperatures`

Récupère la température moyenne pour chaque jour de l'année.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `application/json`

## 4. Endpoints de Statut et Temps Réel

---

### `GET /status`

Fournit un aperçu complet de l'état actuel du système.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `application/json`

---

### `GET /history`

Récupère les données historiques de température et d'humidité.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `application/json`

---

### `GET /safetyStatus`

Récupère des informations détaillées sur l'état du système de sécurité.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `application/json`

## 5. Endpoints de la Caméra

---

### `GET /capture`

Capture et renvoie une image JPEG unique.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `image/jpeg`

---

### `GET /mjpeg`

Démarre un flux vidéo au format MJPEG (Motion JPEG). Idéal pour l'élément `<img>` en HTML.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `multipart/x-mixed-replace`

---

### `GET /mjpeg-info`

Fournit les métadonnées sur le flux MJPEG, comme l'URL du flux et la résolution.

- **Méthode :** `GET`
- **Réponse Succès (200 OK) :** `application/json`
  ```json
  {
    "stream_url": "/mjpeg",
    "width": 320,
    "height": 240
  }
  ```

## 6. Endpoints de Débogage

---

### `GET /setLogLevel`

Change le niveau de verbosité des logs sur le port série.

- **Méthode :** `GET`
- **Paramètres :** `level` (int, requis, 0-4)
- **Réponse Succès (200 OK) :** `text/plain`

---

## 7. Architecture Frontend (JavaScript)

Le code JavaScript a été refactoré pour suivre une architecture modulaire et basée sur un état centralisé. Cela améliore la maintenabilité, la robustesse et la lisibilité du code.

### Fichiers Clés

- **`data/js/state.js`** : **Source de Vérité Unique**. Exporte un objet `state` qui contient toutes les données dynamiques de l'application (statut des capteurs, configuration, état de l'interface). Toute modification de l'état de l'application doit passer par ce module.

- **`data/js/api.js`** : **Couche de Communication**. Centralise toutes les fonctions `fetch` qui communiquent avec l'API de l'ESP32. Ce module est le seul à dialoguer avec le serveur. Il retourne des promesses avec les données JSON parsées.

- **`data/js/main.js`** : **Point d'Entrée**. Orchestre le démarrage de l'application. Ses responsabilités sont :
  1. Charger la configuration initiale via `api.js` et la stocker dans `state.js`.
  2. Initialiser tous les modules UI.
  3. Démarrer la boucle de mise à jour périodique qui rafraîchit l'état.

- **`data/js/ui/` (Répertoire)** : Contient tous les modules dédiés à la manipulation du DOM. Chaque fichier est responsable d'une partie spécifique de l'interface.
  - `tabs.js`: Gère la navigation par onglets.
  - `surveillance.js`: Met à jour les cartes de statut et les graphiques de l'onglet "Surveillance".
  - `camera.js`: Gère l'affichage et les erreurs du flux vidéo.
  - `configuration.js`: Gère les formulaires de l'onglet "Configuration".
  - `profiles.js`: Gère la liste des profils, leur chargement et leur sauvegarde.
  - `seasonal.js`: Gère la heatmap et l'éditeur de courbe saisonnière.
  - `led.js`: Gère les contrôles de la LED (couleur, intensité).

### Flux de Données

1.  `main.js` lance une mise à jour périodique.
2.  La fonction de mise à jour appelle `api.getStatus()`.
3.  Une fois les données reçues, elles sont stockées dans l'objet `state` via une fonction du module `state.js`.
4.  Les modules UI, qui observent l'état ou sont appelés après une mise à jour, lisent les nouvelles données depuis `state` et mettent à jour le DOM en conséquence.

Ce flux unidirectionnel (API → State → UI) rend l'application plus prévisible et plus facile à débugger.