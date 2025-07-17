/**
 * @file main.js
 * @description Point d'entrée principal de l'application.
 * Orchestre l'initialisation et les mises à jour périodiques.
 */

import { state, updateStatus, setConfig } from './state.js';
import { api } from './api.js';
import { initTabs } from './ui/tabs.js';
import { updateSurveillance } from './ui/surveillance.js';
import { initCamera } from './ui/camera.js';
import { initConfiguration } from './ui/configuration.js';
import { initProfiles } from './ui/profiles.js';
import { initSeasonal } from './ui/seasonal.js';
import { initLedControls } from './ui/led.js';

// --- FONCTIONS DE MISE À JOUR ---

async function periodicUpdate() {
    try {
        const newStatus = await api.getStatus();
        updateStatus(newStatus);
        updateSurveillance(state.status);
    } catch (error) {
        console.error("Erreur lors de la mise à jour périodique:", error);
    }
}

// --- INITIALISATION ---

document.addEventListener('DOMContentLoaded', async () => {
    console.log("🚀 Initialisation de l'application...");

    try {
        // 1. Charger la configuration initiale
        const initialConfig = await api.getCurrentConfig();
        setConfig(initialConfig);

        // 2. Initialiser les modules UI
        initTabs((newTab) => {
            state.ui.currentTab = newTab;
            // Logique à exécuter lors du changement d'onglet si nécessaire
        });
        
        initCamera(state.config);
        initConfiguration(state.config, async (newConfig) => {
            await api.applyAllSettings(newConfig);
            setConfig(newConfig);
        });
        initProfiles(async (profileName) => {
            const profile = await api.loadProfile(profileName);
            setConfig(profile);
            // Mettre à jour l'UI de configuration avec le nouveau profil
        });
        initSeasonal(state.config);
        initLedControls(state.config);

        // 3. Démarrer les mises à jour périodiques
        setInterval(periodicUpdate, 2000); // Toutes les 2 secondes
        await periodicUpdate(); // Premier appel immédiat

        console.log("✅ Application initialisée avec succès.");

    } catch (error) {
        console.error("❌ Erreur critique lors de l'initialisation:", error);
        // Afficher un message d'erreur à l'utilisateur
        const body = document.querySelector('body');
        body.innerHTML = '<div style="color: red; text-align: center; padding: 20px;">' +
                         '<h1>Erreur de connexion</h1>' +
                         '<p>Impossible de charger la configuration initiale de l\'appareil.</p>' +
                         '<p>Veuillez vérifier la connexion et rafraîchir la page.</p>' +
                         '</div>';
    }
});