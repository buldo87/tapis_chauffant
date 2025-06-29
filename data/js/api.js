// js/api.js
// Fonction pour la mise à jour de la courbe
function updateTemperatureCurve(newTemperatureData) {
    if (typeof window.temperatureData !== 'undefined' && typeof window.chart !== 'undefined') {
        window.temperatureData = newTemperatureData;
        window.chart.data.datasets[0].data = [...window.temperatureData];
        window.chart.update();
        
        if (typeof window.updateTempGrid === 'function') window.updateTempGrid();
        if (typeof window.updateStatus === 'function') window.updateStatus();
        
        return true;
    }
    return false;
}

// Fonction retourne les données sans essayer de mettre à jour l'interface
async function fetchLocalData() {
    const lat = document.getElementById('latInput').value;
    const lon = document.getElementById('lonInput').value;

    const today = new Date();
    const date = today.toISOString().split('T')[0];
    const thisYear = new Date().getFullYear();

    if (!lat || !lon) {
        throw new Error("Latitude et longitude manquantes dans les paramètres");
    }

    console.log(`Récupération données météo pour ${lat}, ${lon} à la date ${date}`);

    const years = [thisYear - 4, thisYear - 3, thisYear - 2, thisYear - 1];
    const tempData = [];
    const humData = [];

    try {
        for (let year of years) {
            console.log(`Récupération année ${year}...`);
            
            try {
                const data = await fetchData(year, date, lat, lon);

                if (data.hourly && data.hourly.temperature_2m && data.hourly.temperature_2m.length === 24) {
                    tempData.push(data.hourly.temperature_2m);
                    if (data.hourly.relative_humidity_2m && data.hourly.relative_humidity_2m.length === 24) {
                        humData.push(data.hourly.relative_humidity_2m);
                    }
                } else {
                    console.warn(`Données incomplètes pour ${year}`);
                }
            } catch (yearError) {
                console.warn(`Erreur pour l'année ${year}:`, yearError.message);
                // Continuer avec les autres années
                continue;
            }
        }

        if (tempData.length === 0) {
            throw new Error("Aucune donnée météo disponible pour cette période et ces coordonnées");
        }

        const avgTemps = averageArrays(tempData);
        const avgHumidity = humData.length > 0 ? averageArrays(humData) : null;

        console.log("Températures moyennes calculées:", avgTemps);
/*
        // Conversion des températures pour le terrarium
        const processedTemps = avgTemps.map(temp => {
            // Utiliser globalMinTempSet et globalMaxTempSet pour la sécurité du terrarium
            return Math.max(globalMinTempSet, Math.min(globalMaxTempSet, Math.round(temp * 10) / 10));
        }); 
*/
        // Conversion des températures pour le terrarium sans limitation arrondi à un chiffre après la virgule
        const processedTemps = avgTemps.map(temp => {
            // Retourner simplement la température arrondie
            return Math.round(temp * 10) / 10;
        });
        console.log("Températures traitées pour le terrarium:", processedTemps);

        return {
            temperatures: processedTemps,
            humidity: avgHumidity,
            rawTemperatures: avgTemps,
            success: true,
            dataPoints: tempData.length
        };

    } catch (error) {
        console.error("Erreur récupération données météo:", error);
        throw error;
    }
}

// Fonction pour récupérer les données d'une année
async function fetchData(year, date, lat, lon) {
    const [y, m, d] = date.split("-");
    const dateStr = `${year}-${m.padStart(2, '0')}-${d.padStart(2, '0')}`;
    const url = `https://archive-api.open-meteo.com/v1/archive?latitude=${lat}&longitude=${lon}&start_date=${dateStr}&end_date=${dateStr}&hourly=temperature_2m,relative_humidity_2m&timezone=Europe%2FParis`;

    console.log(`Requête API: ${url}`);

    try {
        const res = await fetch(url);
        if (!res.ok) {
            throw new Error(`HTTP ${res.status}: ${res.statusText}`);
        }
        
        const data = await res.json();
        
        // Validation des données reçues
        if (!data.hourly) {
            throw new Error(`Pas de données horaires pour ${year}`);
        }
        
        if (!data.hourly.temperature_2m || data.hourly.temperature_2m.length !== 24) {
            throw new Error(`Données de température incomplètes pour ${year} (${data.hourly.temperature_2m ? data.hourly.temperature_2m.length : 0} points au lieu de 24)`);
        }
        
        return data;
    } catch (error) {
        console.error(`Erreur API météo pour ${year}:`, error);
        throw error;
    }
}

// Fonction pour calculer la moyenne de plusieurs tableaux
function averageArrays(arrays) {
    if (!arrays || arrays.length === 0) {
        throw new Error("Aucun tableau fourni pour le calcul de moyenne");
    }
    
    const n = arrays.length;
    const result = [];
    
    for (let i = 0; i < 24; i++) {
        let sum = 0;
        let validValues = 0;
        
        for (let arr of arrays) {
            if (arr[i] !== null && arr[i] !== undefined && !isNaN(arr[i])) {
                sum += arr[i];
                validValues++;
            }
        }
        
        if (validValues > 0) {
            result.push(sum / validValues);
        } else {
            // Valeur par défaut si aucune donnée valide
            result.push(i >= 6 && i <= 18 ? 25 : 22); // Jour: 25°C, Nuit: 22°C
        }
    }
    
    return result;
}

// Validation des coordonnées
function validateAndGetCoordinates() {
    const lat = document.getElementById('latInput').value;
    const lon = document.getElementById('lonInput').value;
    
    if (!lat || !lon) {
        // Tentative de géolocalisation automatique
        return new Promise((resolve, reject) => {
            if (navigator.geolocation) {
                navigator.geolocation.getCurrentPosition(
                    (position) => {
                        const detectedLat = position.coords.latitude.toFixed(4);
                        const detectedLon = position.coords.longitude.toFixed(4);
                        
                        document.getElementById('latInput').value = detectedLat;
                        document.getElementById('lonInput').value = detectedLon;
                        
                        resolve({ lat: detectedLat, lon: detectedLon });
                    },
                    (error) => reject(new Error("Coordonnées manquantes et géolocalisation échouée"))
                );
            } else {
                reject(new Error("Coordonnées manquantes et géolocalisation non supportée"));
            }
        });
    }
    
    return Promise.resolve({ lat, lon });
}
