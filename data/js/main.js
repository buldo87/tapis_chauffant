// js/main.js

/**
 * Gestionnaire principal de l'application Terrarium
 * Coordonne toutes les fonctionnalités et gère l'interface utilisateur
 */
// Variables globales
let globalMinTempSet = 0;
let globalMaxTempSet = 40;
let tempChart, humidityChart;
let refreshInterval;
let cameraEnabled = false;

// Tab switching 
function initTabs() {
	const tabButtons = document.querySelectorAll('.tab-button');
	const dashboardContent = document.getElementById('dashboardContent');
	const configContent = document.getElementById('configContent');
	
	tabButtons.forEach(button => {
		button.addEventListener('click', () => {
			tabButtons.forEach(btn => btn.className = btn.className.replace('tab-active', 'tab-inactive'));
			button.className = button.className.replace('tab-inactive', 'tab-active');
			
			if (button.id === 'tabDashboard') {
				dashboardContent.style.display = 'block';
				configContent.style.display = 'none';
			} else {
				dashboardContent.style.display = 'none';
				configContent.style.display = 'block';
			}
		});
	});
}

// Data update function fetch
function updateData() {
	fetch("/temperature")
		.then(res => res.text())
		.then(data => {
			if (data !== "--") {
				const timeLabel = new Date().toLocaleTimeString();
				const tempValue = parseFloat(data);
				if (!isNaN(tempValue)) {
					tempChart.data.labels.push(timeLabel);
					tempChart.data.datasets[0].data.push(tempValue);
					
					// Calculate moving average for last 24h (assuming 1 point per 5 seconds = 17280 points per day)
					const windowSize = Math.min(tempChart.data.datasets[0].data.length, 1440); // 24h at 1 point per minute
					const movingAvg = calculateMovingAverage(tempChart.data.datasets[0].data, windowSize);
					tempChart.data.datasets[1].data = movingAvg;
					
					if (tempChart.data.labels.length > 1000) {
						tempChart.data.labels.shift();
						tempChart.data.datasets[0].data.shift();
						tempChart.data.datasets[1].data.shift();
					}
					tempChart.update('none');
					document.getElementById("temperature").innerText = data;
				}
			}
		})
		.catch(err => console.error("Erreur lecture température:", err));

	fetch("/humidity")
		.then(res => res.text())
		.then(data => {
			if (data !== "--") {
				const timeLabel = new Date().toLocaleTimeString();
				const humidityValue = parseFloat(data);
				if (!isNaN(humidityValue)) {
					humidityChart.data.labels.push(timeLabel);
					humidityChart.data.datasets[0].data.push(humidityValue);
					
					// Calculate moving average for humidity
					const windowSize = Math.min(humidityChart.data.datasets[0].data.length, 1440);
					const movingAvg = calculateMovingAverage(humidityChart.data.datasets[0].data, windowSize);
					humidityChart.data.datasets[1].data = movingAvg;
					
					if (humidityChart.data.labels.length > 1000) {
						humidityChart.data.labels.shift();
						humidityChart.data.datasets[0].data.shift();
						humidityChart.data.datasets[1].data.shift();
					}
					humidityChart.update('none');
					document.getElementById("humidity").innerText = data;
				}
			}
		})
		.catch(err => console.error("Erreur lecture humidité:", err));

	fetch("/heaterState")
		.then(res => res.text())
		.then(data => {
			const element = document.getElementById("heaterState");
			element.innerText = data;

			const pwmValue = parseInt(data);
			if (pwmValue > 60) {
				element.className = "metric-value heater-high";
			} else if (pwmValue > 20 || (!document.getElementById("usePWM").checked && pwmValue > 0)) {
				element.className = "metric-value heater-medium";
			} else {
				element.className = "metric-value heater-low";
			}
		})
		.catch(err => console.error("Erreur lecture état chauffage:", err));

	// Update other data 
	fetch("/currentTime")
		.then(res => res.text())
		.then(data => document.getElementById("currentTime").innerText = data)
		.catch(err => console.error("Erreur lecture heure:", err));

	fetch("/setGlobalMaxTemp")
		.then(res => res.text())
		.then(data => document.getElementById("maxTempSet").innerText = data)
		.catch(err => console.error("Erreur lecture temp max:", err));

	fetch("/setGlobalMinTemp")
		.then(res => res.text())
		.then(data => document.getElementById("minTempSet").innerText = data)
		.catch(err => console.error("Erreur lecture temp min:", err));

    fetch("/minHumidite")
        .then(res => {
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            return res.text();
        })
        .then(data => document.getElementById("minHumidite").innerText = data)
        .catch(err => {
            console.warn("Endpoint /minHumidite non disponible:", err.message);
            document.getElementById("minHumidite").innerText = "--";
        });

    fetch("/maxHumidite")
        .then(res => {
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            return res.text();
        })
        .then(data => document.getElementById("maxHumidite").innerText = data)
        .catch(err => {
            console.warn("Endpoint /maxHumidite non disponible:", err.message);
            document.getElementById("maxHumidite").innerText = "--";
        });
	
	fetch("/maxTemperature")
        .then(res => {
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            return res.text();
        })
        .then(data => document.getElementById("maxTemperature").innerText = data)
        .catch(err => {
            console.warn("Endpoint /maxTemperature non disponible:", err.message);
            document.getElementById("maxTemperature").innerText = "--";
        });
		
	fetch("/minTemperature")
        .then(res => {
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            return res.text();
        })
        .then(data => document.getElementById("minTemperature").innerText = data)
        .catch(err => {
            console.warn("Endpoint /minTemperature non disponible:", err.message);
            document.getElementById("minTemperature").innerText = "--";
        });
	
	fetch("/movingAverageHum")
        .then(res => {
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            return res.text();
        })
        .then(data => document.getElementById("movingAverageHum").innerText = data)
        .catch(err => {
            console.warn("Endpoint /movingAverageHum non disponible:", err.message);
            document.getElementById("movingAverageHum").innerText = "--";
        });
	
	fetch("/movingAverageTemp")
        .then(res => {
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            return res.text();
        })
        .then(data => document.getElementById("movingAverageTemp").innerText = data)
        .catch(err => {
            console.warn("Endpoint /movingAverageTemp non disponible:", err.message);
            document.getElementById("movingAverageTemp").innerText = "--";
        });
}

// Initialize on DOM ready
document.addEventListener('DOMContentLoaded', () => {
	initTabs();
	initCharts();
	    // Valider l'initialisation
    setTimeout(() => {
        validateChartInitialization();
    }, 100);
    
    // Autres initialisations...
    loadFromDevice();
    setInterval(updateCurrentHour, 60000);
    setInterval(updateStatus, 10000);
	// Écouter les événements de changement
	document.getElementById("minTempSet").addEventListener("change", updateGlobalTemps);
	document.getElementById("maxTempSet").addEventListener("change", updateGlobalTemps);
	document.getElementById("useLimitTemp").addEventListener("change", updateVisibility);
	document.getElementById("usePWM").addEventListener("change", updateVisibility);
	document.getElementById("weatherMode").addEventListener("change", updateVisibility);
	document.getElementById("showCamera").addEventListener("change", updateCameraVisibility);
	document.getElementById("applyBtn").addEventListener("click", applyAllSettings);
	
	// LED intensity slider
	document.getElementById("ledIntensity").addEventListener("input", (e) => {
		document.getElementById("ledIntensityValue").textContent = e.target.value + "%";
	});


    // Charger le module de carte
    if (typeof initMap !== 'undefined') {
        console.log('🗺️ Module de carte disponible');
    }
    
    // Gestionnaire pour les changements de coordonnées
    document.getElementById('latInput').addEventListener('change', function() {
        const lat = parseFloat(this.value);
        const lon = parseFloat(document.getElementById('lonInput').value);
        
        if (!isNaN(lat) && !isNaN(lon) && mapVisible && map) {
            centerMapOn(lat, lon);
        }
    });
    
    document.getElementById('lonInput').addEventListener('change', function() {
        const lat = parseFloat(document.getElementById('latInput').value);
        const lon = parseFloat(this.value);
        
        if (!isNaN(lat) && !isNaN(lon) && mapVisible && map) {
            centerMapOn(lat, lon);
        }
    });

	// Load settings
	fetch("/getSettings")
		.then(res => res.json())
		.then(settings => {
			if (settings.hysteresis !== undefined) document.getElementById("hysteresisSet").value = settings.hysteresis;
			if (settings.Kp !== undefined) document.getElementById("KpSet").value = settings.Kp;
			if (settings.Ki !== undefined) document.getElementById("KiSet").value = settings.Ki;
			if (settings.Kd !== undefined) document.getElementById("KdSet").value = settings.Kd;
			if (settings.weatherMode !== undefined) document.getElementById("weatherMode").checked = settings.weatherMode;
			if (settings.usePWM !== undefined) document.getElementById("usePWM").checked = settings.usePWM;
			if (settings.latitude !== undefined) document.getElementById("latInput").value = settings.latitude;
			if (settings.longitude !== undefined) document.getElementById("lonInput").value = settings.longitude;
			if (settings.minTempSet !== undefined) document.getElementById("minTempSet").value = settings.minTempSet;
			if (settings.maxTempSet !== undefined) document.getElementById("maxTempSet").value = settings.maxTempSet;			
			const enabled = settings.cameraEnabled || false;
			document.getElementById("showCamera").checked = enabled;
			cameraEnabled = enabled;

			updateVisibility();
			updateCameraVisibility();
		})
		.catch(err => console.error("Erreur récupération paramètres:", err));

	// Load historical data and start updates
	loadHistory();
	updateData();
	updateGlobalTemps();
	setInterval(updateData, 5000); // Update every 30 seconds
	
});
