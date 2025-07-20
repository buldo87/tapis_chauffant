import { parseBinData } from '../utils.js';
import { api } from '../api.js';
import { state, setConfig } from '../state.js';
import { updateTempChart } from './configuration.js';
import { updateHeatmap } from './seasonal.js';

const profileList = document.getElementById('profileList');

async function refreshProfiles() {
    try {
        const profiles = await api.listProfiles();
        renderProfileList(profiles);
    } catch (error) {
        console.error("Failed to refresh profiles:", error);
        profileList.innerHTML = '<li class="text-red-500">Erreur de chargement des profils.</li>';
    }
}

function renderProfileList(profiles) {
    profileList.innerHTML = '';
    if (!profiles || profiles.length === 0) {
        profileList.innerHTML = '<li>Aucun profil trouvé.</li>';
        return;
    }

    profiles.forEach(profileName => {
        const li = document.createElement('li');
        li.className = 'flex justify-between items-center p-2 bg-gray-800 rounded';
        li.innerHTML = `
            <span>${profileName}</span>
            <div>
                <button class="btn-sm btn-primary load-profile" data-name="${profileName}" title="Charger"><i class="fas fa-folder-open"></i></button>
                <button class="btn-sm btn-danger delete-profile" data-name="${profileName}" title="Supprimer"><i class="fas fa-trash"></i></button>
                <button class="btn-sm btn-info download-profile" data-name="${profileName}" title="Télécharger"><i class="fas fa-download"></i></button>
            </div>
        `;
        profileList.appendChild(li);
    });
}

async function downloadBothFiles(profileName) {
    // Trigger download for general.json
    const jsonLink = document.createElement('a');
    jsonLink.href = `/downloadProfile?name=${encodeURIComponent(profileName)}`;
    jsonLink.download = `${profileName}.json`;
    document.body.appendChild(jsonLink);
    jsonLink.click();
    document.body.removeChild(jsonLink);

    // Trigger download for temperature.bin after a short delay
    setTimeout(() => {
        const binLink = document.createElement('a');
        binLink.href = `/downloadSeasonalData?name=${encodeURIComponent(profileName)}`;
        binLink.download = `${profileName}.bin`;
        document.body.appendChild(binLink);
        binLink.click();
        document.body.removeChild(binLink);
    }, 500); // Small delay to ensure first download starts
}

async function loadProfile(name) {
    try {
        // 1. Charger les données de configuration générale du profil (JSON)
        const profileConfig = await api.loadProfile(name);
        // Divide temperature values by 10 for frontend use
        profileConfig.setpoint /= 10.0;
        profileConfig.globalMinTempSet /= 10.0;
        profileConfig.globalMaxTempSet /= 10.0;
        profileConfig.tempCurve = profileConfig.tempCurve.map(t => t / 10.0);
        setConfig(profileConfig);

        // 2. Charger les données de température annuelles (.bin) pour ce profil
        // L'API utilise le profil courant stocké sur l'ESP, donc il faut l'activer d'abord
        await api.activateProfile(name); 
        const yearlyDataBuffer = await api.getYearlyTemperatures();
        
        // 3. Parser les données .bin et mettre à jour la heatmap
        const yearlyData = (await parseBinData(yearlyDataBuffer)).map(day => day.map(t => t / 10.0)); // Divide by 10 here
        updateHeatmap(yearlyData);

        // Mettre à jour la courbe de température du jour actuel dans la configuration
        // Pour l'instant, on prend le premier jour du profil chargé
        if (yearlyData && yearlyData.length > 0) {
            state.config.tempCurve = yearlyData[0];
            updateTempChart(state.config.tempCurve);
        }

        alert(`Profil '${name}' chargé et activé.`);
        document.getElementById('activeProfileName').textContent = name;

    } catch (error) {
        console.error(`Failed to load profile ${name}:`, error);
        alert(`Erreur lors du chargement du profil '${name}'.`);
    }
}

async function saveProfile() {
    const name = prompt("Entrez un nom pour le profil:");
    if (!name) return;

    try {
        await api.saveProfile({ ...state.config, name });
        alert(`Profil '${name}' sauvegardé.`);
        refreshProfiles();
    } catch (error) {
        console.error(`Failed to save profile ${name}:`, error);
        alert(`Erreur lors de la sauvegarde du profil '${name}'.`);
    }
}

async function deleteProfile(name) {
    if (!confirm(`Voulez-vous vraiment supprimer le profil '${name}' ?`)) return;

    try {
        await api.deleteProfile(name);
        alert(`Profil '${name}' supprimé.`);
        refreshProfiles();
    } catch (error) {
        console.error(`Failed to delete profile ${name}:`, error);
        alert(`Erreur lors de la suppression du profil '${name}'.`);
    }
}

export function initProfiles() {
    refreshProfiles();

    profileList.addEventListener('click', (e) => {
        if (e.target.classList.contains('load-profile') || e.target.closest('.load-profile')) {
            const name = e.target.dataset.name || e.target.closest('.load-profile').dataset.name;
            loadProfile(name);
        }
        if (e.target.classList.contains('delete-profile') || e.target.closest('.delete-profile')) {
            const name = e.target.dataset.name || e.target.closest('.delete-profile').dataset.name;
            deleteProfile(name);
        }
        if (e.target.classList.contains('download-profile') || e.target.closest('.download-profile')) {
            const name = e.target.dataset.name || e.target.closest('.download-profile').dataset.name;
            downloadBothFiles(name);
        }
    });

    window.saveProfile = saveProfile;
    window.refreshProfileList = refreshProfiles;
}