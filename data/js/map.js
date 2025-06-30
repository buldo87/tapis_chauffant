// js/map.js - Carte Leaflet pour la géolocalisation
let leafletMapInstance = null;
let leafletMarker = null;
let isMapVisible = false;

// Initialisation de la carte
function initMap() {
    if (leafletMapInstance) {
        return; // Carte déjà initialisée
    }
    
    // Position par défaut (Paris)
    const defaultLat = 48.8566;
    const defaultLon = 2.3522;
    
    // Récupérer les coordonnées actuelles ou utiliser les valeurs par défaut
    const currentLat = parseFloat(document.getElementById('latInput').value) || defaultLat;
    const currentLon = parseFloat(document.getElementById('lonInput').value) || defaultLon;
    
    // Créer la carte
    leafletMapInstance = L.map('map').setView([currentLat, currentLon], 10);
    
    // Ajouter les tuiles OpenStreetMap (gratuit)
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© OpenStreetMap contributors',
        maxZoom: 18
    }).addTo(leafletMapInstance);
    
    // Ajouter un marqueur à la position actuelle
    if (currentLat && currentLon) {
        addMarker(currentLat, currentLon);
    }
    
    // Gestionnaire de clic sur la carte
    leafletMapInstance.on('click', function(e) {
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
    if (leafletMarker) {
        leafletMapInstance.removeLayer(leafletMarker);
    }
    
    // Créer un nouveau marqueur
    leafletMarker = L.marker([lat, lon], {
        draggable: true
    }).addTo(leafletMapInstance);
    
    // Popup avec les coordonnées
    leafletMarker.bindPopup(`
        <b>📍 Position sélectionnée</b><br>
        Latitude: ${lat.toFixed(4)}<br>
        Longitude: ${lon.toFixed(4)}
    `).openPopup();
    
    // Gestionnaire de drag du marqueur
    leafletMarker.on('dragend', function(e) {
        const newPos = e.target.getLatLng();
        updateCoordinates(newPos.lat, newPos.lng);
        
        // Mettre à jour le popup
        leafletMarker.bindPopup(`
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
    
    if (!isMapVisible) {
        // Afficher la carte
        mapContainer.style.display = 'block';
        toggleBtn.innerHTML = '🗺️ Fermer la Carte';
        isMapVisible = true;
        
        // Initialiser la carte si nécessaire
        if (!leafletMapInstance) {
            setTimeout(() => {
                initMap();
                // Redimensionner la carte après affichage
                leafletMapInstance.invalidateSize();
            }, 100);
        } else {
            // Redimensionner la carte existante
            setTimeout(() => {
                leafletMapInstance.invalidateSize();
            }, 100);
        }
        
    } else {
        // Masquer la carte
        mapContainer.style.display = 'none';
        toggleBtn.innerHTML = '🗺️ Ouvrir la Carte';
        isMapVisible = false;
    }
}

// Centrer la carte sur une position
function centerMapOn(lat, lon, zoom = 12) {
    if (leafletMapInstance) {
        leafletMapInstance.setView([lat, lon], zoom);
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
            if (isMapVisible && leafletMapInstance) {
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
                
                if (isMapVisible && leafletMapInstance) {
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