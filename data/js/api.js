/**
 * @file api.js
 * @description Centralise toutes les communications avec l'API du serveur ESP32.
 */

async function fetchJson(url, options = {}) {
    try {
        const response = await fetch(url, options);
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return await response.json();
    } catch (error) {
        console.error(`Fetch error for ${url}:`, error);
        throw error;
    }
}

async function postJson(url, data) {
    return await fetchJson(url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(data)
    });
}

export const api = {
    /**
     * Récupère l'état actuel des capteurs et du chauffage.
     * @returns {Promise<object>} L'objet de statut.
     */
    getStatus: () => fetchJson('/status'),

    /**
     * Récupère la configuration complète du système.
     * @returns {Promise<object>} L'objet de configuration.
     */
    getCurrentConfig: () => fetchJson('/getCurrentConfig'),

    /**
     * Applique et sauvegarde une nouvelle configuration.
     * @param {object} config - Le nouvel objet de configuration.
     * @returns {Promise<Response>} La réponse du serveur.
     */
    applyAllSettings: (config) => postJson('/applyAllSettings', config),

    /**
     * Récupère la liste des profils de configuration disponibles.
     * @returns {Promise<Array<object>>} La liste des profils.
     */
    listProfiles: () => fetchJson('/listProfiles').then(data => data.profiles),

    /**
     * Charge un profil de configuration par son nom.
     * @param {string} name - Le nom du profil.
     * @returns {Promise<object>} Les données du profil.
     */
    loadProfile: (name) => fetchJson(`/loadProfile?name=${encodeURIComponent(name)}`),

    /**
     * Sauvegarde un profil de configuration.
     * @param {object} profileData - Les données du profil à sauvegarder.
     * @returns {Promise<Response>} La réponse du serveur.
     */
    saveProfile: (profileData) => postJson('/saveProfile', profileData),

    /**
     * Supprime un profil par son nom.
     * @param {string} name - Le nom du profil.
     * @returns {Promise<Response>} La réponse du serveur.
     */
    deleteProfile: (name) => fetch(`/deleteProfile?name=${encodeURIComponent(name)}`, { method: 'DELETE' }),

    /**
     * Active un profil par son nom.
     * @param {string} name - Le nom du profil.
     * @returns {Promise<Response>} La réponse du serveur.
     */
    activateProfile: (name) => fetch(`/activateProfile?name=${encodeURIComponent(name)}`),

    /**
     * Récupère les données de température pour un jour spécifique d'un profil saisonnier.
     * @param {number} dayIndex - L'index du jour (0-365).
     * @returns {Promise<object>} Les données de température du jour.
     */
    getDayData: (dayIndex) => fetchJson(`/getDayData?day=${dayIndex}`),

    /**
     * Sauvegarde les données de température pour un jour spécifique.
     * @param {number} dayIndex - L'index du jour.
     * @param {Array<number>} temperatures - Le tableau des 24 températures.
     * @returns {Promise<Response>} La réponse du serveur.
     */
    saveDayData: (dayIndex, temperatures) => postJson('/saveDayData', { day: dayIndex, temperatures }),

    /**
     * Récupère les données de température annuelles pour la heatmap.
     * @returns {Promise<object>} Les données annuelles.
     */
    getYearlyTemperatures: () => fetchJson('/getYearlyTemperatures'),

    /**
     * Récupère les informations sur le stream MJPEG.
     * @returns {Promise<object>} Les informations du stream.
     */
    getMjpegInfo: () => fetchJson('/mjpeg-info')
};