
let map = null;
let currentMarker = null;

function initMap(initialLat, initialLon) {
    if (map) return;

    map = L.map('map').setView([initialLat, initialLon], 10);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© OpenStreetMap contributors'
    }).addTo(map);

    addMarker(initialLat, initialLon);

    map.on('click', function(e) {
        const { lat, lng } = e.latlng;
        updateCoordinates(lat, lng);
        addMarker(lat, lng);
    });
}

function addMarker(lat, lon) {
    if (currentMarker) {
        map.removeLayer(currentMarker);
    }
    currentMarker = L.marker([lat, lon]).addTo(map);
}

function updateCoordinates(lat, lon) {
    document.getElementById('latInput').value = lat.toFixed(4);
    document.getElementById('lonInput').value = lon.toFixed(4);
}

export function toggleMap(show) {
    const mapContainer = document.getElementById('mapContainer');
    if (show) {
        mapContainer.style.display = 'block';
        if (!map) {
            const lat = parseFloat(document.getElementById('latInput').value) || 48.8566;
            const lon = parseFloat(document.getElementById('lonInput').value) || 2.3522;
            initMap(lat, lon);
        }
        setTimeout(() => map.invalidateSize(), 100); // Fix map rendering issues
    } else {
        mapContainer.style.display = 'none';
    }
}
