//main.js
let globalMinTempSet = 0;
let globalMaxTempSet = 40;
let tempChart, humidityChart;
let refreshInterval; 
let cameraEnabled = false;
let cameraResolution = "qvga";

const brightnessValue = document.getElementById('brightness-value');
const ledDot = document.getElementById('led-dot');
const brightnessSlider = document.getElementById('brightness-slider');
const ledToggle = document.getElementById('led-toggle');
const colorPicker = document.getElementById('color-picker');

const resolutionDimensions = {
    'qvga': { width: 320, height: 240 },
    'vga': { width: 640, height: 480 },
    'svga': { width: 800, height: 600 }
};

function setStreamDimensions(resolution) {
    const dimensions = resolutionDimensions[resolution];
    const camStream = document.getElementById('camStream');
    if (dimensions && camStream) {
        camStream.style.width = `${dimensions.width}px`;
        camStream.style.height = 'auto'; // Garde le ratio, important pour la responsivité
        camStream.style.maxWidth = '100%'; // Assure que l'image ne dépasse pas son conteneur
    }
}

class ConfigManager {
    constructor() {
        this.config = {};
        this.isLoaded = false;
    }

    async loadConfig() {
        if (this.isLoaded) return this.config;
        
        try {
            const response = await fetch('/getCurrentConfig');
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            
            this.config = await response.json();
            this.isLoaded = true;
            
            // Synchroniser les variables globales
            this.syncGlobalVariables();
            
            return this.config;
        } catch (error) {
            console.error('Erreur chargement config:', error);
            // Utiliser des valeurs par défaut en cas d'erreur
            this.config = this.getDefaultConfig();
            this.isLoaded = true;
            return this.config;
        }
    }

    syncGlobalVariables() {
        // Synchroniser avec les variables globales existantes
        globalMinTempSet = this.config.globalMinTempSet || 15;
        globalMaxTempSet = this.config.globalMaxTempSet || 35;
        cameraEnabled = this.config.cameraEnabled || false;
        cameraResolution = this.config.cameraResolution || "qvga";
    }

    getDefaultConfig() {
        return {
            hysteresis: 0.3,
            Kp: 2.0, Ki: 5.0, Kd: 1.0,
            usePWM: false,
            globalMinTempSet: 15,
            globalMaxTempSet: 35,
            latitude: 48.85,
            longitude: 2.35,
            weatherModeEnabled: false,
            cameraEnabled: false,
            useLimitTemp: true,
            tempCurve: Array(24).fill(22),
            ledState: false,
            ledBrightness: 255,
            ledRed: 255, ledGreen: 255, ledBlue: 255
        };
    }

    async saveConfig(updates = {}) {
        this.config = { ...this.config, ...updates };
        
        try {
            const response = await fetch('/applyAllSettings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(this.config)
            });
            
            if (!response.ok) throw new Error('Sauvegarde échouée');
            return await response.text();
        } catch (error) {
            console.error('Erreur sauvegarde:', error);
            throw error;
        }
    }

    get(key) { return this.config[key]; }
    set(key, value) { this.config[key] = value; }
}

// Instance globale
const configManager = new ConfigManager();

class UpdateManager {
    constructor() {
        this.intervals = [];
    }

    startPeriodicUpdates() {
        console.log('🔄 Démarrage des mises à jour périodiques...');
        
        // Vérifier que les fonctions existent avant de les appeler
        if (typeof updateData === 'function') {
            this.intervals.push(setInterval(() => updateData(), 5000));
        }
        
        if (typeof updateCurrentHour === 'function') {
            this.intervals.push(setInterval(() => updateCurrentHour(), 60000));
        }
        
        if (typeof updateStatus === 'function') {
            this.intervals.push(setInterval(() => updateStatus(), 10000));
        }
        
        // Démarrer immédiatement une première mise à jour
        setTimeout(() => {
            if (typeof updateData === 'function') updateData();
            if (typeof updateStatus === 'function') updateStatus();
        }, 1000);
    }

    stopAllUpdates() {
        this.intervals.forEach(id => clearInterval(id));
        this.intervals = [];
        console.log('⏹️ Toutes les mises à jour périodiques arrêtées');
    }
}

// Dans main.js - ajouter cette fonction
function updateGlobalTemps() {
    const minTempInput = document.getElementById("minTempSet");
    const maxTempInput = document.getElementById("maxTempSet");
    
    if (minTempInput && maxTempInput) {
        globalMinTempSet = parseFloat(minTempInput.value) || 0;
        globalMaxTempSet = parseFloat(maxTempInput.value) || 40;
        
        // Synchroniser avec le ConfigManager
        configManager.set('globalMinTempSet', globalMinTempSet);
        configManager.set('globalMaxTempSet', globalMaxTempSet);
        
        console.log(`Températures mises à jour : Min = ${globalMinTempSet}, Max = ${globalMaxTempSet}`);
    }
}

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

	fetch("/status")
	  .then(res => res.json())
	  .then(data => {
		const heaterEl = document.getElementById("heaterState");
		const modeEl = document.getElementById("currentMode");
		const consigneEl = document.getElementById("consigneTemp");
		const modeDetailsEl = document.getElementById("modeDetails");

		// Chauffage (output PWM)
		if (data.heaterState !== undefined) {
			const pwmValue = parseInt(data.heaterState);
		  if (data.usePWM) {
			
			heaterEl.innerText = `${data.heaterState}`;
			if (pwmValue > 100) {
				heaterEl.className = "metric-value heater-high";
			} else if (pwmValue > 20 || (!document.getElementById("usePWM").checked && pwmValue > 0)) {
				heaterEl.className = "metric-value heater-medium";
			} else {
				heaterEl.className = "metric-value heater-low";
			}

		  } else {
			heaterEl.innerText = data.heaterState > 0 ? "ON" : "OFF";
			if (pwmValue > 100) {
				heaterEl.className = "metric-value heater-high";
			} else {
				heaterEl.className = "metric-value heater-low";
			}	
		  }
		}

		// Consigne
		if (data.consigne !== undefined) {
		  consigneEl.innerText = `${data.consigne.toFixed(1)}°C`;
		}

		// Mode & détails
		if (data.usePWM) {
		  modeEl.innerText = "PWM";
		  modeDetailsEl.innerText = `Kp=${data.Kp}, Ki=${data.Ki}, Kd=${data.Kd}`;
		} else {
		  modeEl.innerText = "ON/OFF";
		  modeDetailsEl.innerText = `Hystérésis = ${data.hysteresis}`;
		}
		console.log("🔍 /status JSON:", data);
	  })
	  .catch(err => {
		console.warn("Erreur /status :", err.message);
		document.getElementById("heaterState").innerText = "--";
		document.getElementById("currentMode").innerText = "--";
		document.getElementById("consigneTemp").innerText = "--°C";
		document.getElementById("modeDetails").innerText = "";
	  });
}

// Update LED dot color and brightness
function updateLedDot() {
	const brightness = brightnessSlider.value;
	const color = $("#color-picker").spectrum("get").toHexString();

	if (ledToggle.checked) {
        // La bordure prend la couleur vive
        ledDot.style.border = `2px solid ${color}`;

        // Le fond utilise RGBA pour gérer l'opacité du remplissage
        const r = parseInt(color.substr(1, 2), 16);
        const g = parseInt(color.substr(3, 2), 16);
        const b = parseInt(color.substr(5, 2), 16);
        const opacity = brightness / 255;
        
        ledDot.style.backgroundColor = `rgba(${r}, ${g}, ${b}, ${opacity})`;
        // On s'assure que l'opacité globale de l'élément est à 1 pour que la bordure ne soit pas affectée
        ledDot.style.opacity = 1;

    } else {
        // Si la LED est éteinte, pas de bordure et fond noir
        ledDot.style.border = 'none';
        ledDot.style.backgroundColor = '#000';
        ledDot.style.opacity = 1;
    }
}

// Fonction pour convertir RGB en hexadécimal
function rgbToHex(r, g, b) {
	return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

let cameraInterval = null;
let isCameraRunning = false;

function cameraRefreshLoop() {
    if (!isCameraRunning) return; // Arrêter la boucle si la caméra est désactivée

    const camStream = document.getElementById('camStream');
    const tempImg = new Image();

    // Si l'image charge avec succès...
    tempImg.onload = () => {
        camStream.src = tempImg.src; // Mettre à jour l'image visible
        setTimeout(cameraRefreshLoop, 1000); // Programmer la prochaine image dans 1 seconde
    };

    // Si une erreur survient...
    tempImg.onerror = () => {
        console.warn("Erreur de chargement de l'image du flux, nouvelle tentative dans 2s...");
        setTimeout(cameraRefreshLoop, 2000); // Réessayer dans 2 secondes
    };

    // Lancer le chargement de l'image
    tempImg.src = '/capture?' + new Date().getTime();
}

function updateCameraVisibility() {
    const cameraContainer = document.getElementById('cameraContainer');
    const showCamera = document.getElementById('showCamera');

    if (showCamera.checked) {
        cameraContainer.style.display = 'block';
        if (!isCameraRunning) {
            isCameraRunning = true;
            cameraRefreshLoop(); // Démarrer la boucle de rafraîchissement
        }
    } else {
        cameraContainer.style.display = 'none';
        isCameraRunning = false; // Arrêter la boucle au prochain tour
    }
}

// Centraliser les événements
function initEventListeners() {
    // Gestion sliders et switches
    const minTempInput = document.getElementById("minTempSet");
    const maxTempInput = document.getElementById("maxTempSet");
    
    if (minTempInput) minTempInput.addEventListener("change", updateGlobalTemps);
    if (maxTempInput) maxTempInput.addEventListener("change", updateGlobalTemps);
    
    const useLimitTemp = document.getElementById("useLimitTemp");
    if (useLimitTemp) {
        useLimitTemp.addEventListener("change", () => {
            if (typeof reapplyTemperatureLimits === 'function') reapplyTemperatureLimits();
            if (typeof updateVisibility === 'function') updateVisibility();
        });
    }
    
    const usePWM = document.getElementById("usePWM");
    if (usePWM) usePWM.addEventListener("change", () => {
        if (typeof updateVisibility === 'function') updateVisibility();
    });
    
    const weatherMode = document.getElementById("weatherMode");
    if (weatherMode) weatherMode.addEventListener("change", () => {
        if (typeof updateVisibility === 'function') updateVisibility();
    });
    
    const showCamera = document.getElementById("showCamera");
    if (showCamera) {
        showCamera.addEventListener("change", updateCameraVisibility);
        // Appel initial pour définir la visibilité au chargement
        updateCameraVisibility(); 
    }
    
    const applyBtn = document.getElementById("applyBtn");
    if (applyBtn) applyBtn.addEventListener("click", () => {
        if (typeof applyAllSettings === 'function') applyAllSettings();
    });

    const brightnessSlider = document.getElementById('brightness-slider');
    const brightnessValue = document.getElementById('brightness-value');
    if (brightnessSlider && brightnessValue) {
        brightnessSlider.addEventListener('input', () => {
            brightnessValue.textContent = brightnessSlider.value;
        });
    }

    // Appel initial pour la visibilité
    if (typeof updateVisibility === 'function') {
        updateVisibility();
    }

    const cameraResolutionSelector = document.getElementById('cameraResolution');
    if (cameraResolutionSelector) {
        cameraResolutionSelector.addEventListener('change', (event) => {
            if (typeof setStreamDimensions === 'function') {
                setStreamDimensions(event.target.value);
            }
        });
    }
}

// Fonction l'initialisation de Spectrum
function initSpectrum() {
    return new Promise((resolve) => {
        const colorPicker = $("#color-picker");

        if (!colorPicker.length) {
            console.warn("❌ Élément #color-picker non trouvé dans le DOM.");
            return resolve(false);
        }

        // Initialisation du color picker
        colorPicker.spectrum({
            preferredFormat: "hex",
            showInput: true,
            showInitial: true,
            showPalette: false,
            showButtons: false,
            cancelText: 'Annuler',
            chooseText: 'Choisir',
            change: updateLedDot
        });

        // 🔁 Attente active jusqu'à ce que .get() fonctionne
        const checkReady = () => {
            const pickerInstance = colorPicker.spectrum("get");
            if (pickerInstance && typeof pickerInstance.toHexString === "function") {
                console.log("✅ Spectrum initialisé avec succès");
                resolve(true);
            } else {
                setTimeout(checkReady, 100);
            }
        };

        checkReady();
    });
}

// Dans main.js - Modifier l'ordre d'initialisation
document.addEventListener('DOMContentLoaded', async () => {
    try {
        console.log('🚀 Démarrage de l\'application...');

        // 1. Charger la configuration une seule fois
        await configManager.loadConfig();
        console.log('✅ Configuration chargée');

        // 2. Initialiser l'interface de base
        initTabs();

        // 3. Initialiser les graphiques
        if (typeof initCharts === 'function') {
            initCharts();
        }

        // 4. Initialiser le graphique de température
        if (typeof initChart === 'function') {
            await initChart();
        }

        // 5. ✅ ATTENDRE que Spectrum soit initialisé
        const ready = await initSpectrum();
        if (!ready) {
            alert("Erreur : le color picker n'a pas pu être initialisé.");
            return;
        }

        // 6. Maintenant charger l'interface utilisateur avec la config
        if (typeof loadCurrentConfigToUI === 'function') {
            await loadCurrentConfigToUI();
        }

        // 7. Démarrer les mises à jour périodiques
        const updateManager = new UpdateManager();
        updateManager.startPeriodicUpdates();

        // 8. Initialisation des événements
        initEventListeners();

        console.log('✅ Initialisation complète terminée');
    } catch (error) {
        console.error('❌ Erreur initialisation:', error);
        alert('Erreur lors de l\'initialisation: ' + error.message);
    }
});




