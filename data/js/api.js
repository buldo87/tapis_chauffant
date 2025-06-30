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
// js/map.js
let map = null;
let currentMarker = null;
let mapVisible = false;

// Initialisation de la carte
function initMap() {
    if (map) {
        return; // Carte déjà initialisée
    }
    
    // Position par défaut (Paris)
    const defaultLat = 48.8566;
    const defaultLon = 2.3522;
    
    // Récupérer les coordonnées actuelles ou utiliser les valeurs par défaut
    const currentLat = parseFloat(document.getElementById('latInput').value) || defaultLat;
    const currentLon = parseFloat(document.getElementById('lonInput').value) || defaultLon;
    
    // Créer la carte
    map = L.map('map').setView([currentLat, currentLon], 10);
    
    // Ajouter les tuiles OpenStreetMap (gratuit)
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© OpenStreetMap contributors',
        maxZoom: 18
    }).addTo(map);
    
    // Ajouter un marqueur à la position actuelle
    if (currentLat && currentLon) {
        addMarker(currentLat, currentLon);
    }
    
    // Gestionnaire de clic sur la carte
    map.on('click', function(e) {
        const lat = e.latlng.lat;
        const lon = e.latlng.lng;
        
        // Mettre à jour les champs de coordonnées
        updateCoordinates(lat, lon);
        
        // Déplacer le marqueur
        addMarker(lat, lon);
        
        console.log(`📍 Nouvelle position sélectionnée: ${lat.toFixed(4)}, ${lon.toFixed(4)}`);
    });
    
    console.log('🗺️ Carte initialisée avec succès');
}

// Ajouter ou déplacer le marqueur
function addMarker(lat, lon) {
    // Supprimer le marqueur existant
    if (currentMarker) {
        map.removeLayer(currentMarker);
    }
    
    // Créer un nouveau marqueur
    currentMarker = L.marker([lat, lon], {
        draggable: true
    }).addTo(map);
    
    // Popup avec les coordonnées
    currentMarker.bindPopup(`
        <b>📍 Position sélectionnée</b><br>
        Latitude: ${lat.toFixed(4)}<br>
        Longitude: ${lon.toFixed(4)}
    `).openPopup();
    
    // Gestionnaire de drag du marqueur
    currentMarker.on('dragend', function(e) {
        const newPos = e.target.getLatLng();
        updateCoordinates(newPos.lat, newPos.lng);
        
        // Mettre à jour le popup
        currentMarker.bindPopup(`
            <b>📍 Position sélectionnée</b><br>
            Latitude: ${newPos.lat.toFixed(4)}<br>
            Longitude: ${newPos.lng.toFixed(4)}
        `).openPopup();
        
        console.log(`🔄 Marqueur déplacé: ${newPos.lat.toFixed(4)}, ${newPos.lng.toFixed(4)}`);
    });
}

// Mettre à jour les champs de coordonnées
function updateCoordinates(lat, lon) {
    document.getElementById('latInput').value = lat.toFixed(4);
    document.getElementById('lonInput').value = lon.toFixed(4);
    
    // Déclencher l'événement change pour les autres fonctions
    document.getElementById('latInput').dispatchEvent(new Event('change'));
    document.getElementById('lonInput').dispatchEvent(new Event('change'));
}

// Basculer l'affichage de la carte
function toggleMap() {
    const mapContainer = document.getElementById('mapContainer');
    const toggleBtn = document.getElementById('mapToggleBtn');
    
    if (!mapVisible) {
        // Afficher la carte
        mapContainer.style.display = 'block';
        toggleBtn.innerHTML = '🗺️ Fermer la Carte';
        mapVisible = true;
        
        // Initialiser la carte si nécessaire
        if (!map) {
            setTimeout(() => {
                initMap();
                // Redimensionner la carte après affichage
                map.invalidateSize();
            }, 100);
        } else {
            // Redimensionner la carte existante
            setTimeout(() => {
                map.invalidateSize();
            }, 100);
        }
        
    } else {
        // Masquer la carte
        mapContainer.style.display = 'none';
        toggleBtn.innerHTML = '🗺️ Ouvrir la Carte';
        mapVisible = false;
    }
}

// Centrer la carte sur une position
function centerMapOn(lat, lon, zoom = 12) {
    if (map) {
        map.setView([lat, lon], zoom);
        addMarker(lat, lon);
    }
}

// Géolocalisation améliorée
function getCurrentLocation() {
    if (!navigator.geolocation) {
        alert('❌ La géolocalisation n\'est pas supportée par votre navigateur');
        return;
    }
    
    // Afficher un indicateur de chargement
    const btn = event.target;
    const originalText = btn.innerHTML;
    btn.innerHTML = '🔄 Localisation...';
    btn.disabled = true;
    
    navigator.geolocation.getCurrentPosition(
        (position) => {
            const lat = position.coords.latitude;
            const lon = position.coords.longitude;
            const accuracy = position.coords.accuracy;
            
            // Mettre à jour les coordonnées
            updateCoordinates(lat, lon);
            
            // Centrer la carte si elle est visible
            if (mapVisible && map) {
                centerMapOn(lat, lon, 14);
            }
            
            // Restaurer le bouton
            btn.innerHTML = originalText;
            btn.disabled = false;
            
            console.log(`📍 Position détectée: ${lat.toFixed(4)}, ${lon.toFixed(4)} (précision: ${accuracy.toFixed(0)}m)`);
            
            // Afficher une confirmation
            alert(`✅ Position détectée avec succès !\nLatitude: ${lat.toFixed(4)}\nLongitude: ${lon.toFixed(4)}\nPrécision: ${accuracy.toFixed(0)}m`);
        },
        (error) => {
            // Restaurer le bouton
            btn.innerHTML = originalText;
            btn.disabled = false;
            
            let errorMessage = '❌ Erreur de géolocalisation: ';
            switch (error.code) {
                case error.PERMISSION_DENIED:
                    errorMessage += 'Permission refusée par l\'utilisateur';
                    break;
                case error.POSITION_UNAVAILABLE:
                    errorMessage += 'Position non disponible';
                    break;
                case error.TIMEOUT:
                    errorMessage += 'Délai d\'attente dépassé';
                    break;
                default:
                    errorMessage += 'Erreur inconnue';
                    break;
            }
            
            console.warn(errorMessage);
            alert(errorMessage);
        },
        {
            enableHighAccuracy: true,
            timeout: 10000,
            maximumAge: 300000 // 5 minutes
        }
    );
}

// Recherche d'adresse (optionnel - nécessite une API de géocodage)
function searchLocation() {
    const address = prompt('🔍 Entrez une adresse ou nom de lieu:');
    if (!address) return;
    
    // Utilisation de l'API Nominatim (gratuite) pour le géocodage
    const url = `https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(address)}&limit=1`;
    
    fetch(url)
        .then(response => response.json())
        .then(data => {
            if (data && data.length > 0) {
                const result = data[0];
                const lat = parseFloat(result.lat);
                const lon = parseFloat(result.lon);
                
                updateCoordinates(lat, lon);
                
                if (mapVisible && map) {
                    centerMapOn(lat, lon, 12);
                }
                
                alert(`✅ Lieu trouvé: ${result.display_name}\nLatitude: ${lat.toFixed(4)}\nLongitude: ${lon.toFixed(4)}`);
            } else {
                alert('❌ Lieu non trouvé. Essayez avec une adresse plus précise.');
            }
        })
        .catch(error => {
            console.error('Erreur de géocodage:', error);
            alert('❌ Erreur lors de la recherche d\'adresse');
        });
}
