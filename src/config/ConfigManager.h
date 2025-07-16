#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "SystemConfig.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

// La classe ConfigManager gère la configuration du système.
// Elle est conçue comme une classe statique pour un accès simple et direct.
class ConfigManager {
public:
    /**
     * @brief Initialise le gestionnaire de configuration.
     * @return true si l'initialisation a réussi, false sinon.
     */
    static bool initialize();

    /**
     * @brief Charge la configuration depuis la mémoire non volatile.
     * @param config Référence à la structure de configuration à remplir.
     * @return true si le chargement a réussi, false sinon.
     */
    static bool loadConfig(SystemConfig& config);

    /**
     * @brief Sauvegarde la configuration en mémoire non volatile.
     * @param config Référence à la structure de configuration à sauvegarder.
     * @return true si la sauvegarde a réussi, false sinon.
     */
    static bool saveConfig(const SystemConfig& config);

    /**
     * @brief Sauvegarde la configuration uniquement si elle a changé.
     * @param config Référence à la structure de configuration.
     * @return true si la sauvegarde a réussi ou n'était pas nécessaire, false en cas d'erreur.
     */
    static bool saveConfigIfChanged(SystemConfig& config);

    /**
     * @brief Demande une sauvegarde différée de la configuration.
     */
    static void requestSave();

    /**
     * @brief Traite les demandes de sauvegarde différée.
     * @param config Référence à la structure de configuration.
     */
    static void processPendingSave(SystemConfig& config);
    
    // --- Gestion des profils ---

    /**
     * @brief Crée un profil par défaut.
     * @return true si la création a réussi, false sinon.
     */
    static bool createDefaultProfile();

    /**
     * @brief Charge un profil de configuration.
     * @param profileName Nom du profil à charger.
     * @param config Référence à la structure de configuration à mettre à jour.
     * @return true si le chargement a réussi, false sinon.
     */
    static bool loadProfile(const String& profileName, SystemConfig& config);

    /**
     * @brief Sauvegarde la configuration actuelle dans un profil.
     * @param profileName Nom du profil où sauvegarder.
     * @param config Référence à la configuration à sauvegarder.
     * @return true si la sauvegarde a réussi, false sinon.
     */
    static bool saveProfile(const String& profileName, const SystemConfig& config);

    /**
     * @brief Supprime un profil.
     * @param profileName Nom du profil à supprimer.
     * @return true si la suppression a réussi, false sinon.
     */
    static bool deleteProfile(const String& profileName);

    /**
     * @brief Vérifie si un profil existe.
     * @param profileName Nom du profil à vérifier.
     * @return true si le profil existe, false sinon.
     */
    static bool profileExists(const String& profileName);

    /**
     * @brief Liste tous les profils disponibles.
     * @return Un vecteur de chaînes de caractères contenant les noms des profils.
     */
    static std::vector<String> listProfiles();
    
    // --- Gestion des données saisonnières ---

    /**
     * @brief Charge les données de température saisonnières pour un jour donné.
     * @param profileName Nom du profil.
     * @param dayIndex Index du jour dans l'année (0-365).
     * @param temperatures Pointeur vers un tableau de 24 flottants pour stocker les températures.
     * @return true si le chargement a réussi, false sinon.
     */
    static bool loadSeasonalData(const String& profileName, int dayIndex, float* temperatures);

    /**
     * @brief Sauvegarde les données de température saisonnières pour un jour donné.
     * @param profileName Nom du profil.
     * @param dayIndex Index du jour dans l'année (0-365).
     * @param temperatures Pointeur vers un tableau de 24 flottants contenant les températures à sauvegarder.
     * @return true si la sauvegarde a réussi, false sinon.
     */
    static bool saveSeasonalData(const String& profileName, int dayIndex, const float* temperatures);

    /**
     * @brief Crée un fichier de données saisonnières par défaut pour un profil.
     * @param profileName Nom du profil.
     * @return true si la création a réussi, false sinon.
     */
    static bool createDefaultSeasonalData(const String& profileName);
    
private:
    static Preferences prefs;
    static unsigned long lastSaveRequest;
    static bool savePending;
    static const unsigned long SAVE_DELAY = 5000;
    
    static uint32_t calculateConfigHash(const SystemConfig& config);
    static bool ensureProfileDirectory(const String& profileName);
    static void generateDefaultDayTemperatures(int dayIndex, float* dayTemps);
};

#endif