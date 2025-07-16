//configuration.js
 // coube ajustable
let chart;
let temperatureData = Array(24).fill(0).map((_, i) => 22 + Math.sin(i / 24 * Math.PI * 2) * 6); // Courbe sinusoïdale par défaut
let extendedTemperatureData = [temperatureData[temperatureData.length - 1], ...temperatureData, temperatureData[0]];
let isDragging = false;
let currentHour = new Date().getHours();
let currentMin = new Date().getMinutes();
let refreshing = false;

function updateCameraVisibility() {
	const show = document.getElementById("showCamera").checked;
	const container = document.getElementById("cameraContainer");
	const img = document.getElementById("camStream");

	container.style.display = show ? "block" : "none";
	cameraEnabled = show;

	fetch("/setCamera?enabled=" + (show ? 1 : 0))
		.catch(err => console.error("Erreur setCamera:", err));

	if (show && !refreshing) {
		startImageLoop(img);
	}
}

function startImageLoop(img, interval = 1000) {
	refreshing = true;

	const loadNextImage = () => {
		if (!cameraEnabled) {
			refreshing = false;
			return;
		}

		const ts = Date.now();
		const newImg = new Image();

		newImg.onload = () => {
			// Remplacer l'image visible seulement après chargement
			img.src = newImg.src;
			setTimeout(loadNextImage, interval);
		};

		newImg.onerror = () => {
			console.warn("Erreur chargement image");
			setTimeout(loadNextImage, interval * 2);
		};

		newImg.src = `/capture?ts=${ts}`;
	};

	loadNextImage();
}

function setResolution() {
    var quality = document.getElementById("cameraResolution").value;
    fetch('/set-resolution-cam', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: 'quality=' + encodeURIComponent(quality)
    })
    .then(response => response.text())
    .then(data => {
        alert("Résolution mise à jour : " + data);
    })
    .catch(error => {
        console.error('Erreur:', error);
    });
}
// Fonction pour convertir RGB en hexadécimal
function rgbToHex(r, g, b) {
	return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

function updateVisibility() {
    // PWM settings
    const pwmChecked = document.getElementById("usePWM").checked;
    const pwmDiv = document.getElementById("pwmSettings");
    const hysteresisDiv = document.getElementById("hysteresisSettings");

    if (pwmDiv) pwmDiv.style.display = pwmChecked ? "block" : "none";
    if (hysteresisDiv) hysteresisDiv.style.display = pwmChecked ? "none" : "block";

    // Weather settings
    const weatherChecked = document.getElementById("weatherMode").checked;
    const weatherDiv = document.getElementById("weatherSettings");
    const copieButton = document.getElementById("copieSettings");

    if (weatherDiv) weatherDiv.style.display = weatherChecked ? "block" : "none";
    if (copieButton) copieButton.style.display = weatherChecked ? "inline-block" : "none";

    // Temperature limit settings
    const limitTempChecked = document.getElementById("useLimitTemp").checked;
    const limitTempDiv = document.getElementById("limitTempSettings");

    if (limitTempDiv) limitTempDiv.style.display = limitTempChecked ? "block" : "none";

    // Seasonal visibility - Appeler la fonction dédiée
    if (typeof updateSeasonalVisibility === 'function') {
        updateSeasonalVisibility();
    }
}

// Dans configuration.js - Modifier loadCurrentConfigToUI()
async function loadCurrentConfigToUI() {
    try {
        const res = await fetch('/getCurrentConfig');
        if (!res.ok) throw new Error("Erreur lors de la lecture de la configuration");

        const config = await res.json();

        // Remplissage des champs standards
        document.getElementById("hysteresisSet").value = config.hysteresis;
        document.getElementById("KpSet").value = config.Kp;
        document.getElementById("KiSet").value = config.Ki;
        document.getElementById("KdSet").value = config.Kd;
        document.getElementById("usePWM").checked = !!config.usePWM;
        document.getElementById("latInput").value = config.latitude;
        document.getElementById("lonInput").value = config.longitude;
        document.getElementById("weatherMode").checked = !!config.weatherModeEnabled;
        document.getElementById("showCamera").checked = !!config.cameraEnabled;
        document.getElementById("cameraResolution").value = config.cameraResolution;
        document.getElementById("useLimitTemp").checked = !!config.useLimitTemp;
        document.getElementById("maxTempSet").value = config.globalMaxTempSet;
        document.getElementById("minTempSet").value = config.globalMinTempSet;
        const activeProfileEl = document.getElementById("activeProfileName");
        if (activeProfileEl) {
            activeProfileEl.textContent = config.currentProfileName || 'default';
        }
        // Nouveau: Charger l'état du mode saisonnier
        const seasonalModeElement = document.getElementById("seasonalMode");
        if (seasonalModeElement) {
            seasonalModeElement.checked = !!config.seasonalModeEnabled;
        }

        // 🔧 Configuration LED - SANS appeler saveConfigurationled()
        const ledToggle = document.getElementById("led-toggle");
        const brightnessSlider = document.getElementById("brightness-slider");
        const brightnessValue = document.getElementById("brightness-value");

        if (ledToggle && brightnessSlider && brightnessValue) {
            ledToggle.checked = !!config.ledState;
            brightnessSlider.value = config.ledBrightness;
            brightnessValue.textContent = config.ledBrightness;

            // Attendre que Spectrum soit prêt avant de configurer la couleur
            if (typeof config.ledRed === "number" && 
                typeof config.ledGreen === "number" && 
                typeof config.ledBlue === "number") {
                
                const hexColor = rgbToHex(config.ledRed, config.ledGreen, config.ledBlue);
                
                // Attendre l'initialisation de Spectrum
                setTimeout(() => {
                    try {
                        const colorPicker = $("#color-picker");
                        if (colorPicker.length && typeof colorPicker.spectrum === 'function') {
                            colorPicker.spectrum("set", hexColor);
                            updateLedDot();
                        }
                    } catch (error) {
                        console.warn("⚠️ Impossible de configurer la couleur LED:", error.message);
                    }
                }, 1000); // Délai pour laisser Spectrum s'initialiser
            }
        } else {
            console.warn("⚠️ Élément LED manquant dans le DOM");
        }

        // 📈 Charger la courbe de température
        if (Array.isArray(config.tempCurve)) {
            temperatureData = config.tempCurve;
            updateChartAndGrid();
        }

        // Mettre à jour la visibilité des sections
        updateVisibility();

        // Mettre à jour la taille de l'image de la caméra
        if (typeof setStreamDimensions === 'function') {
            setStreamDimensions(config.cameraResolution);
        }

        console.log("✅ Configuration rechargée depuis l'ESP");
    } catch (e) {
        console.error("❌ Erreur de lecture config :", e);
    }
}

// Save all settings
function applyAllSettings() {
    console.log("🔄 Début de la sauvegarde de la configuration...");

    const get = (id) => document.getElementById(id);

    const fields = [
        "hysteresisSet", "KpSet", "KiSet", "KdSet",
        "usePWM", "latInput", "lonInput",
        "weatherMode", "seasonalMode", "showCamera", "cameraResolution", "useLimitTemp",
        "maxTempSet", "minTempSet",
        "led-toggle", "brightness-slider", "color-picker"
    ];

    // 🔍 Vérification des éléments requis
    for (const id of fields) {
        if (!get(id)) {
            alert(`❌ Élément ${id} introuvable`);
            console.error(`❌ Élément ${id} introuvable`);
            return;
        }
    }

    // ✅ Conversion couleur LED en RGB
    let red = 255, green = 255, blue = 255;
    let color = "#ffffff";
    try {
        color = $("#color-picker").spectrum("get").toHexString();
        red = parseInt(color.substr(1, 2), 16);
        green = parseInt(color.substr(3, 2), 16);
        blue = parseInt(color.substr(5, 2), 16);
    } catch (e) {
        console.warn("⚠️ Erreur de récupération couleur, valeurs par défaut utilisées.");
    }

    // 📦 Création de l'objet de configuration global
    const payload = {
        hysteresis: parseFloat(get("hysteresisSet").value),
        Kp: parseFloat(get("KpSet").value),
        Ki: parseFloat(get("KiSet").value),
        Kd: parseFloat(get("KdSet").value),
        usePWM: get("usePWM").checked ? 1 : 0,
        globalMinTempSet: parseFloat(get("minTempSet").value),
        globalMaxTempSet: parseFloat(get("maxTempSet").value),
        latitude: parseFloat(get("latInput").value),
        longitude: parseFloat(get("lonInput").value),
        weatherModeEnabled: get("weatherMode").checked ? 1 : 0,
        seasonalModeEnabled: get("seasonalMode").checked ? 1 : 0, // Nouveau
        cameraEnabled: get("showCamera").checked ? 1 : 0,
		cameraResolution: get("cameraResolution").value,
        useLimitTemp: get("useLimitTemp").checked ? 1 : 0,
        tempCurve: [...temperatureData],  
        ledState: get("led-toggle").checked,
        ledBrightness: parseInt(get("brightness-slider").value),
        ledRed: red,
        ledGreen: green,
        ledBlue: blue,
        logLevel: parseInt(get("logLevel").value)
    };

    console.log("📊 Configuration complète à envoyer :");
    console.log(payload);

    // Envoi vers serveur
    fetch("/applyAllSettings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
    })
    .then(res => {
        if (!res.ok) throw new Error("Échec serveur : " + res.status);
        return res.text();
    })
    .then(txt => {
        console.log("✅ Réponse du serveur :", txt);
        const btn = get("applyBtn");
        btn.innerHTML = '<i class="fas fa-check mr-2"></i>Sauvegardé !';
        btn.style.background = '#4ade80';
        setTimeout(() => {
            btn.innerHTML = '<i class="fas fa-download mr-2"></i>Sauvegarder Configuration';
            btn.style.background = '';
        }, 2000);

        setTimeout(() => {
            if (typeof loadCurrentConfigToUI === "function") {
                loadCurrentConfigToUI();
            }
        }, 1000);
    })
    .catch(err => {
        console.error("❌ Erreur lors de l'application :", err);
        alert("Erreur : " + err.message);
    });
}

// Load historical data
function loadHistory() {
	fetch("/history")
		.then(res => res.json())
		.then(data => {
			const timeLabels = data.map(entry => new Date(entry.t * 1000).toLocaleTimeString());
			const temperatures = data.map(entry => entry.temp);
			const humidities = data.map(entry => entry.hum);

			tempChart.data.labels = timeLabels;
			tempChart.data.datasets[0].data = temperatures;
			
			// Calculate moving averages for historical data
			const tempMovingAvg = calculateMovingAverage(temperatures, Math.min(temperatures.length, 1440));
			tempChart.data.datasets[1].data = tempMovingAvg;
			tempChart.update();

			humidityChart.data.labels = timeLabels;
			humidityChart.data.datasets[0].data = humidities;
			
			const humMovingAvg = calculateMovingAverage(humidities, Math.min(humidities.length, 1440));
			humidityChart.data.datasets[1].data = humMovingAvg;
			humidityChart.update();
		})
		.catch(err => console.error("Erreur chargement historique:", err));
}

// Propagation des valeurs extrèmes de la courbe
function updateTemperatureFromExtendedIndex(index, temp) {
    // Propagation depuis extendedTemperatureData vers temperatureData
    if (index === 0) {
        // 23h
        updateTemperature(23, temp);
    } else if (index === 25) {
        // 0h
        updateTemperature(0, temp);
    } else if (index >= 1 && index <= 24) {
        // index 1 → 0h, index 2 → 1h, ..., index 24 → 23h
        updateTemperature(index - 1, temp);
    } else {
        console.warn("Index étendu hors bornes :", index);
    }
}

// Configuration du graphique (à placer avant la fonction initChart)
function initChart() {
    const canvas = document.getElementById('configTempChart');
    if (!canvas) {
        console.warn("⚠️ Élément canvas introuvable, tentative retardée");
        setTimeout(initChart, 500);
        return;
    }

    const ctx = canvas.getContext('2d');
    if (!ctx) {
        console.warn("⚠️ Contexte canvas introuvable");
        return;
    }

    // 🔁 Créer les données étendues
    extendedTemperatureData = [
        temperatureData[temperatureData.length - 1], // pour lisser la courbe
        ...temperatureData,
        temperatureData[0]
    ];

    const labels = ['23h', '0h', '1h', '2h', '3h', '4h', '5h', '6h', '7h', '8h', '9h', '10h', '11h',
                    '12h', '13h', '14h', '15h', '16h', '17h', '18h', '19h', '20h', '21h', '22h', '23h', '0h'];

    const chartConfig = {
        type: 'line',
        data: {
            labels: labels,
            datasets: [{
                label: 'Température (°C)',
                data: extendedTemperatureData,
                borderColor: '#ff6b6b',
                backgroundColor: 'rgba(255, 107, 107, 0.1)',
                borderWidth: 3,
                fill: true,
                tension: 0.3,
                pointBackgroundColor: '#ff6b6b',
                pointBorderColor: '#ffffff',
                pointBorderWidth: 2,
                pointRadius: 8,
                pointHoverRadius: 12
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                intersect: false,
                mode: 'index'
            },
            plugins: {
                legend: {
                    labels: {
                        color: '#ffffff',
                        font: { size: 14 }
                    }
                },
                title: {
                    display: true,
                    text: 'Courbe d\'édition du jour - Cliquez et glissez pour ajuster',
                    color: '#ffffff',
                    font: { size: 18 }
                },
                tooltip: {
                    callbacks: {
                        label: function(context) {
                            return `${context.parsed.y.toFixed(1)}°C`;
                        }
                    }
                }
            },
            scales: {
                x: {
                    grid: { color: 'rgba(255, 255, 255, 0.1)' },
                    ticks: { color: '#ffffff' }
                },
                y: {
                    grid: { color: 'rgba(255, 255, 255, 0.1)' },
                    ticks: { color: '#ffffff' },
                    min: function(context) {
                        return Math.min(...context.chart.data.datasets[0].data) - 1;
                    },
                    max: function(context) {
                        return Math.max(...context.chart.data.datasets[0].data) + 1;
                    }
                }
            },
            onHover: (event, activeElements) => {
                event.native.target.style.cursor = activeElements.length > 0 ? 'pointer' : 'crosshair';
            },
            onClick: (event, activeElements) => {
                if (activeElements.length > 0) {
                    const index = activeElements[0].index;
                    const canvasPosition = Chart.helpers.getRelativePosition(event, chart);
                    const dataY = chart.scales.y.getValueForPixel(canvasPosition.y);
                    if (index >= 0 && index <= 25) {
                        const newTemp = Math.max(globalMinTempSet, Math.min(globalMaxTempSet, dataY));
                        updateTemperatureFromExtendedIndex(index, newTemp);
                    }
                }
            }
        }
    };

    // ✅ Initialiser le graphique
    chart = new Chart(ctx, chartConfig);

    // 🎛️ Ajout des événements
    canvas.addEventListener('mousedown', startDrag);
    canvas.addEventListener('mousemove', drag);
    canvas.addEventListener('mouseup', endDrag);
    canvas.addEventListener('mouseleave', endDrag);

    canvas.addEventListener('touchstart', handleTouchStart, { passive: false });
    canvas.addEventListener('touchmove', handleTouchMove, { passive: false });
    canvas.addEventListener('touchend', handleTouchEnd, { passive: false });

    // 🔁 Initialiser l'affichage
    updateTempGrid();
    updateStatus();

    // 🌐 Rendre accessibles les objets globalement
    window.temperatureChart = chart;
    window.temperatureData = temperatureData;
    window.extendedTemperatureData = extendedTemperatureData;
    window.updateTempGrid = updateTempGrid;
    window.updateStatus = updateStatus;
    window.updateChartAndGrid = updateChartAndGrid;
    window.chart = chart;

    console.log("✅ Graphique de température initialisé avec succès");
}

// Fonctions de gestion du drag 
function startDrag(event) {
    isDragging = true;
    drag(event);
    event.preventDefault();
}

function drag(event) {
    if (!isDragging) return;

    const rect = event.target.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;

    const canvasPosition = Chart.helpers.getRelativePosition(event, chart);
    const dataX = chart.scales.x.getValueForPixel(canvasPosition.x);
    const dataY = chart.scales.y.getValueForPixel(canvasPosition.y);

    // Calculer l'index pour 26 heures
    const index = Math.round(dataX);
    if (index >= 0 && index <= 25) {
        const newTemp = Math.max(globalMinTempSet, Math.min(globalMaxTempSet, dataY));
        updateTemperatureFromExtendedIndex(index, newTemp);
    }
    event.preventDefault();
}

function endDrag(event) {
    isDragging = false;
    event.preventDefault();
}

// Gestion tactile pour mobiles/tablettes
let touchStarted = false;

function handleTouchStart(event) {
    touchStarted = true;
    const touch = event.touches[0];
    const mouseEvent = new MouseEvent('mousedown', {
        clientX: touch.clientX,
        clientY: touch.clientY
    });
    startDrag(mouseEvent);
    event.preventDefault();
}

function handleTouchMove(event) {
    if (!touchStarted) return;

    const touch = event.touches[0];
    const mouseEvent = new MouseEvent('mousemove', {
        clientX: touch.clientX,
        clientY: touch.clientY
    });

    const rect = mouseEvent.target.getBoundingClientRect();
    const x = touch.clientX - rect.left;
    const y = touch.clientY - rect.top;

    const canvasPosition = Chart.helpers.getRelativePosition(mouseEvent, chart);
    const dataX = chart.scales.x.getValueForPixel(canvasPosition.x);
    const dataY = chart.scales.y.getValueForPixel(canvasPosition.y);

    // Calculer l'index pour 26 heures
    const index = Math.round(dataX);
    if (index >= 0 && index <= 25) {
        const newTemp = Math.max(globalMinTempSet, Math.min(globalMaxTempSet, dataY));
        updateTemperatureFromExtendedIndex(index, newTemp);
    }
    event.preventDefault();
}

function handleTouchEnd(event) {
    touchStarted = false;
    const mouseEvent = new MouseEvent('mouseup', {});
    endDrag(mouseEvent);
    event.preventDefault();
}

// Fonction updateTemperature
function updateTemperature(hour, temp) {
    // Validation selon vos limites backend (globalMinTempSet-globalMaxTempSet°C pour la sécurité)
    temp = Math.max(globalMinTempSet, Math.min(globalMaxTempSet, temp));

    // Assurez-vous que l'index est dans la plage correcte pour temperatureData
    if (hour >= 0 && hour < temperatureData.length) {
        temperatureData[hour] = temp;
    } else {
        console.error("Index de l'heure hors limites pour temperatureData");
        return;
    }

    // Mettre à jour les données étendues
    extendedTemperatureData = [temperatureData[temperatureData.length - 1], ...temperatureData, temperatureData[0]];

    // Mettre à jour le graphique
    chart.data.datasets[0].data = [...extendedTemperatureData];
    chart.update('none'); // 'none' pour une animation plus fluide pendant le drag

    // Mettre à jour l'affichage
    // updateTempGrid();
    updateStatus();

    // Exposer la mise à jour globalement
    window.temperatureData = temperatureData;
    window.extendedTemperatureData = extendedTemperatureData;
}

// Fonction updateTempGrid
// ✅ CORRECTION : Créer le conteneur s'il n'existe pas
function updateTempGrid() {
    let grid = document.getElementById('temperatureChart');
    
    if (!grid) {
        // Créer le conteneur manquant
        const configChart = document.getElementById('configTempChart');
        if (configChart) {
            const chartCard = configChart.closest('.card');
            if (chartCard) {
                const tempContainer = document.createElement('div');
                tempContainer.className = 'mt-4';
                tempContainer.innerHTML = `
                    <h4 class="text-md font-medium mb-3 text-gray-300">📊 Aperçu Horaire</h4>
                    <div id="temperatureChart" class="grid grid-cols-6 sm:grid-cols-8 md:grid-cols-12 gap-1"></div>
                `;
                chartCard.appendChild(tempContainer);
                grid = document.getElementById('temperatureChart');
            }
        }
    }
    
    if (!grid) {
        console.warn("⚠️ Impossible de créer le conteneur temperatureChart");
        return;
    }

    // Nettoyer et remplir
    grid.innerHTML = '';
    
    if (typeof window.temperatureData !== 'undefined' && Array.isArray(window.temperatureData)) {
        window.temperatureData.forEach((temp, hour) => {
            const hourControl = document.createElement('div');
            hourControl.className = `p-2 bg-gray-700 rounded text-center cursor-pointer hover:bg-gray-600 transition-colors ${hour === new Date().getHours() ? 'ring-2 ring-blue-500' : ''}`;
            hourControl.innerHTML = `
                <div class="text-xs text-gray-400">${hour}h</div>
                <div class="text-sm font-semibold text-white">${temp.toFixed(1)}°C</div>
            `;

            hourControl.onclick = () => {
                const newTemp = prompt(`Température pour ${hour}h (${globalMinTempSet}-${globalMaxTempSet}°C):`, temp.toFixed(1));
                if (newTemp && !isNaN(newTemp)) {
                    const validTemp = Math.max(globalMinTempSet, Math.min(globalMaxTempSet, parseFloat(newTemp)));
                    if (typeof updateTemperature === 'function') {
                        updateTemperature(hour, validTemp);
                    }
                }
            };

            grid.appendChild(hourControl);
        });
    }
}




// Fonction de validation de l'initialisation
function validateChartInitialization() {
    const checks = [
        { name: 'Canvas element', check: () => document.getElementById('temperatureChart') !== null },
        { name: 'Chart object', check: () => typeof chart !== 'undefined' && chart !== null },
        { name: 'Temperature data', check: () => Array.isArray(temperatureData) && temperatureData.length === 24 },
        { name: 'Global variables', check: () => window.temperatureData && window.temperatureChart },
    ];
    
    const results = checks.map(check => ({
        name: check.name,
        passed: check.check()
    }));
    
    const allPassed = results.every(result => result.passed);
    
    console.log('🔍 Validation de l\'initialisation du graphique:');
    results.forEach(result => {
        console.log(`  ${result.passed ? '✅' : '❌'} ${result.name}`);
    });
    
    if (allPassed) {
        console.log('✅ Graphique entièrement fonctionnel');
    } else {
        console.warn('⚠️ Problèmes détectés dans l\'initialisation');
    }
    
    return allPassed;
}

function applyPreset(preset) {
    switch(preset) {
        case 'mer':
            temperatureData = Array(24).fill(0).map((_, i) => {
                if (i >= 6 && i < 10) return 26;
                if (i >= 10 && i < 16) return 29;
                if (i >= 16 && i < 22) return 26;
                return 24;
            });
            break;
        case 'foret':
            temperatureData = Array(24).fill(0).map((_, i) => {
                if (i >= 6 && i < 10) return 24;
                if (i >= 10 && i < 16) return 28;
                if (i >= 16 && i < 22) return 25;
                return 23;
            });
            break;
        case 'tropical':
            temperatureData = Array(24).fill(0).map((_, i) => {
                return 28 + Math.sin((i - 8) / 24 * Math.PI * 2) * 4;
            });
            break;
        case 'desert':
            temperatureData = Array(24).fill(0).map((_, i) => {
                if (i >= 6 && i < 9) return 27;
                if (i >= 9 && i < 17) return 32;
                if (i >= 17 && i < 23) return 28;
                return 23;
            });
            break;
        case 'copie':
            // Afficher un message de chargement dans la console
            console.log('🌡️ Récupération des données météo en cours...');
            
            // Appeler la fonction de récupération des données météo
            fetchLocalDataAndUpdateCurve()
                .then(() => {
                    console.log('✅ Données météo appliquées avec succès');
                })
                .catch(error => {
                    console.error('❌ Erreur lors de l\'application des données météo:', error);
                    alert('Erreur lors de la récupération des données météo: ' + error.message);
                });
            return; // Sortir ici car la mise à jour se fait de manière asynchrone
        case 'constant':
            temperatureData = Array(24).fill(25);
            break;
        default:
            // Mise à jour pour tous les autres cas

            break;
    }
    
    reapplyTemperatureLimits();
    updateChartAndGrid();
}

// Fonction helper pour centraliser la mise à jour 
function updateChartAndGrid() {
    extendedTemperatureData = [temperatureData[temperatureData.length - 1], ...temperatureData, temperatureData[0]];
    if (typeof chart !== 'undefined' && chart !== null) {
        chart.data.datasets[0].data = [...extendedTemperatureData];
        chart.update();
        console.log("📊 Graphique mis à jour avec nouvelles données");
    } else {
        console.warn("⚠️ Graphique non initialisé pour la mise à jour");
    }
    
    if (typeof updateTempGrid === 'function') {
        // updateTempGrid();
    } else {
        console.warn("⚠️ Fonction updateTempGrid non disponible");
    }
    
    if (typeof updateStatus === 'function') {
        updateStatus();
    } else {
        console.warn("⚠️ Fonction updateStatus non disponible");
    }
    
    // Mettre à jour les variables globales
    window.extendedTemperatureData = extendedTemperatureData;
    window.temperatureData = temperatureData;
    window.temperatureChart = chart;
}

// Nouvelle fonction pour gérer la récupération et mise à jour
async function fetchLocalDataAndUpdateCurve() {
    try {
        // Vérifier que les coordonnées sont disponibles
        const lat = document.getElementById('latInput').value;
        const lon = document.getElementById('lonInput').value;

        if (!lat || !lon) {
            throw new Error("Veuillez saisir les coordonnées latitude/longitude dans les paramètres météo");
        }

        // Appeler la fonction de récupération des données
        const result = await fetchLocalData();

        // Mettre à jour la courbe avec les données récupérées
        if (result && result.temperatures) {
            let temperatures = result.temperatures;
            const minTemp = Math.min(...temperatures);
            const maxTemp = Math.max(...temperatures);

            // Vérifier les limites de température
            if (minTemp < globalMinTempSet || maxTemp > globalMaxTempSet) {
                alert(`⚠️ Attention: Les températures dépassent les limites définies (${globalMinTempSet}°C - ${globalMaxTempSet}°C). Les valeurs seront ajustées.`);
                // Appliquer les limitations
                temperatures = temperatures.map(temp => Math.max(globalMinTempSet, Math.min(globalMaxTempSet, temp)));
            }

            temperatureData = temperatures;
            updateChartAndGrid();

            // Afficher un message de succès
            const adjustedMinTemp = Math.min(...temperatureData).toFixed(1);
            const adjustedMaxTemp = Math.max(...temperatureData).toFixed(1);
            alert(`✅ Courbe mise à jour avec les données météo !\nTempératures ajustées: ${adjustedMinTemp}°C à ${adjustedMaxTemp}°C`);
        }

        return result;
    } catch (error) {
        console.error('Erreur dans fetchLocalDataAndUpdateCurve:', error);
        throw error;
    }
}

// Fonctions pour l'indicateur de chargement
function showLoadingIndicator() {
    const indicator = document.createElement('div');
    indicator.id = 'loadingIndicator';
    indicator.innerHTML = '<div style="text-align: center; padding: 20px; color: #fff;">Traitement des données</div>';
    document.body.appendChild(indicator);
}

function hideLoadingIndicator() {
    const indicator = document.getElementById('loadingIndicator');
    if (indicator) {
        indicator.remove();
    }
}

function smoothCurve() {
	// Lissage avec moyenne mobile
	const smoothed = [...temperatureData];
	for (let i = 1; i < 23; i++) {
		smoothed[i] = (temperatureData[i-1] + temperatureData[i] + temperatureData[i+1]) / 3;
	}
	temperatureData = smoothed;
    let extendedTemperatureData = [temperatureData[temperatureData.length - 1], ...temperatureData, temperatureData[0]];
	chart.data.datasets[0].data = [...extendedTemperatureData];
	chart.update();
	updateTempGrid();
}

function resetToDefault() {
     
	temperatureData = Array(24).fill(0).map((_, i) => 24 + Math.sin(i-6 / 24 * Math.PI * 2) * 6);
    let extendedTemperatureData = [temperatureData[temperatureData.length - 1], ...temperatureData, temperatureData[0]];
	chart.data.datasets[0].data = [...extendedTemperatureData];
	chart.update();
	updateTempGrid();
	updateStatus();
}

// ✅ sauvegarder le profil
async function saveProfile() {
    try {
        // 1. Demander le nom du profil
        const name = prompt('Nom du profil :', 'Profil_' + new Date().toISOString().slice(0,10));
        if (!name || !/^[\w\d _-]+$/.test(name)) {
            alert('⛔ Nom invalide (lettres, chiffres, espace, - et _ autorisés)');
            return;
        }

        // 2. Détection du mode saisonnier et présence des données
        const seasonalMode = document.getElementById("seasonalMode")?.checked || false;
        const isSeasonalProfile = seasonalMode && (Array.isArray(window.seasonalData) && window.seasonalData.length === 366);

        // 3. Construction du profil général (general.json)
        const get = (id) => document.getElementById(id);
        const color = $("#color-picker").spectrum("get")?.toHexString() || "#ffffff";
        let ledRed = 255, ledGreen = 255, ledBlue = 255;
        try {
            ledRed = parseInt(color.substr(1, 2), 16);
            ledGreen = parseInt(color.substr(3, 2), 16);
            ledBlue = parseInt(color.substr(5, 2), 16);
        } catch (e) { /* defaults already set */ }

        // Récupération de la courbe 24h
        let temperatureData = window.temperatureData || Array(24).fill(22);

        // Construction de l'objet profil - NOUVEAU FORMAT
        const profile = {
            name: name,
            timestamp: new Date().toISOString(),
            version: "2.0",
            profileType: isSeasonalProfile ? "saisonnier" : "journalier",

            // Températures journalières (courbe 24h)
            temperatures: Array.from(temperatureData),

            // Paramètres de régulation
            usePWM: get("usePWM")?.checked || false,
            hysteresis: parseFloat(get("hysteresisSet")?.value ?? 0.3),
            Kp: parseFloat(get("KpSet")?.value ?? 2.0),
            Ki: parseFloat(get("KiSet")?.value ?? 5.0),
            Kd: parseFloat(get("KdSet")?.value ?? 1.0),

            // Limites
            useLimitTemp: get("useLimitTemp")?.checked || false,
            globalMinTempSet: parseFloat(get("minTempSet")?.value ?? 15),
            globalMaxTempSet: parseFloat(get("maxTempSet")?.value ?? 35),

            // Météo
            weatherModeEnabled: get("weatherMode")?.checked || false,
            latitude: parseFloat(get("latInput")?.value ?? 48.85),
            longitude: parseFloat(get("lonInput")?.value ?? 2.35),

            // Saisonnier
            seasonalModeEnabled: seasonalMode,

            // Caméra
            cameraEnabled: get("showCamera")?.checked || false,
            cameraResolution: get("cameraResolution")?.value ?? "qvga",

            // LED
            ledState: get("led-toggle")?.checked || false,
            ledBrightness: parseInt(get("brightness-slider")?.value ?? 255),
            ledRed, ledGreen, ledBlue,

            // Debug
            debugModeEnabled: get("debugMode")?.checked || false
        };

        // 4. Ajout des données saisonnières si présentes
        if (isSeasonalProfile) {
            if (window.seasonalData.every(day =>
                Array.isArray(day) && day.length === 24 && day.every(v => typeof v === 'number' && !isNaN(v))
            )) {
                // Deep copy pour éviter toute mutation ultérieure
                profile.seasonalData = JSON.parse(JSON.stringify(window.seasonalData));
            } else {
                alert("⚠️ Les données saisonnières sont corrompues ou incomplètes. Le profil sera sauvegardé sans la courbe annuelle !");
            }
        }

        // 5. Envoi au backend avec la nouvelle structure
        const response = await fetch('/saveProfile', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(profile)
        });

        if (!response.ok) {
            const errorText = await response.text();
            throw new Error(errorText);
        }

        const result = await response.text();
        alert('✅ ' + result);
        
        // Rafraîchir la liste des profils
        if (typeof refreshProfileList === "function") refreshProfileList();
        
    } catch (error) {
        alert('❌ Erreur lors de la sauvegarde : ' + (error.message || error));
        if (window.debugMode) console.error(error);
    }
}

// ✅ Fonction unifiée pour sauvegarder les profils
async function saveProfileToBackend(profileData) {
    try {
        const response = await fetch('/saveProfile', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(profileData)
        });
        
        if (response.status === 507) {
            // Espace insuffisant - déclencher le nettoyage interactif
            const errorData = await response.json();
            if (typeof handleInsufficientSpace === 'function') {
                handleInsufficientSpace(errorData.requiredBytes);
            } else {
                alert(`❌ Espace insuffisant: ${errorData.message}`);
            }
            return;
        }
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const result = await response.text();
        alert('✅ ' + result);
        
        // Rafraîchir la liste des profils
        if (typeof refreshProfileList === 'function') {
            refreshProfileList();
        }
        
    } catch (error) {
        console.error('❌ Erreur sauvegarde profil:', error);
        alert('❌ Erreur lors de la sauvegarde: ' + error.message);
    }
}

// ✅ Rafraîchir la liste HTML des profils dans la page
// ✅ Liste des profils mise à jour
async function refreshProfileList() {
    try {
        const response = await fetch('/listProfiles');
        const data = await response.json();
        const profiles = data.profiles;
        
        const ul = document.getElementById('profileList');
        ul.innerHTML = '';
        
        profiles.forEach(profile => {
            const li = document.createElement('li');
            li.classList.add('flex', 'justify-between', 'items-center', 'border-b', 'pb-1');
            
            const sizeInfo = profile.generalSize ? 
                `${Math.round(profile.generalSize/1024)}KB` + 
                (profile.hasTempData ? ` + ${Math.round(profile.tempSize/1024)}KB` : '') : '';
            
            li.innerHTML = `
                <div class="flex-1">
                    <div class="font-medium">${profile.name}</div>
                    <div class="text-xs text-gray-500">
                        ${profile.hasTempData ? '🌍 Avec données saisonnières' : '📊 Journalier'} • ${sizeInfo}
                    </div>
                </div>
                <div class="flex space-x-1">
                    <button onclick="loadNamedProfile('${profile.name}')" title="Charger" class="text-blue-400">📂</button>
                    <button onclick="activateProfile('${profile.name}')" title="Activer" class="text-green-400">✅</button>
                    <button onclick="renameProfile('${profile.name}')" title="Renommer" class="text-yellow-400">✏️</button>
                    <button onclick="deleteProfile('${profile.name}')" title="Supprimer" class="text-red-400">🗑️</button>
                </div>
            `;
            ul.appendChild(li);
        });
        
    } catch (error) {
        console.error('Erreur liste profils:', error);
    }
}

// ✅ Activer un profil
async function activateProfile(name) {
    try {
        const response = await fetch(`/activateProfile?name=${encodeURIComponent(name)}`);
        if (response.ok) {
            alert(`✅ Profil '${name}' activé !`);
            // Recharger la configuration
            await loadCurrentConfigToUI();
            refreshProfileList();
        } else {
            alert('❌ Erreur activation profil');
        }
    } catch (error) {
        alert('❌ Erreur: ' + error.message);
    }
}


// ✅ Fonction pour déterminer le type de profil avec la nouvelle structure
function getProfileType(profile) {
    if (typeof profile === 'string') {
        return '📊 Journalier';
    }
    
    if (profile.profileType === 'saisonnier' || profile.seasonalModeEnabled) {
        return '🌍 Saisonnier';
    }
    
    return '⚙️ Journalier';
}

// ✅ Fonction utilitaire pour formater la taille
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}


// ✅ Fonction pour appliquer un profil complet à l'interface
async function applyCompleteProfileToUI(profileData) {
    try {
        // 1. Températures 24h (courbe du jour)
        if (Array.isArray(profileData.temperatures) && profileData.temperatures.length === 24) {
            window.temperatureData = [...profileData.temperatures];
            if (typeof updateChartAndGrid === 'function') updateChartAndGrid();
        }

        // 2. Saisonnier : seasonalData (366 x 24), si présent
        if (Array.isArray(profileData.seasonalData) && profileData.seasonalData.length === 366) {
            // Validation rapide des sous-tableaux
            let isValid = profileData.seasonalData.every(day =>
                Array.isArray(day) && day.length === 24 && day.every(val => typeof val === "number" && !isNaN(val))
            );
            if (isValid) {
                window.seasonalData = profileData.seasonalData.map(day => [...day]);
                if (typeof createHeatmap === "function") createHeatmap(window.seasonalData);
            } else {
                alert("⚠️ Courbe saisonnière du profil corrompue : elle ne sera pas appliquée.");
            }
        }

        // 3. Paramètres de régulation
        setElementValue("usePWM", profileData.usePWM, 'checkbox');
        setElementValue("hysteresisSet", profileData.hysteresis);
        setElementValue("KpSet", profileData.Kp);
        setElementValue("KiSet", profileData.Ki);
        setElementValue("KdSet", profileData.Kd);

        // 4. Limites
        setElementValue("useLimitTemp", profileData.useLimitTemp, 'checkbox');
        setElementValue("minTempSet", profileData.globalMinTempSet);
        setElementValue("maxTempSet", profileData.globalMaxTempSet);

        // 5. Météo
        setElementValue("weatherMode", profileData.weatherModeEnabled, 'checkbox');
        setElementValue("latInput", profileData.latitude);
        setElementValue("lonInput", profileData.longitude);

        // 6. Saisonnier (switch UI)
        setElementValue("seasonalMode", profileData.seasonalModeEnabled, 'checkbox');
        // Optionnel : forcer la visibilité saisonnière
        if (typeof updateSeasonalVisibility === "function") updateSeasonalVisibility();

        // 7. Caméra
        setElementValue("showCamera", profileData.cameraEnabled, 'checkbox');
        setElementValue("cameraResolution", profileData.cameraResolution);

        // 8. LED
        setElementValue("led-toggle", profileData.ledState, 'checkbox');
        setElementValue("brightness-slider", profileData.ledBrightness);
        setElementValue("brightness-value", profileData.ledBrightness, 'text');
        // Couleur LED
        if (
            typeof profileData.ledRed === "number" &&
            typeof profileData.ledGreen === "number" &&
            typeof profileData.ledBlue === "number"
        ) {
            const hexColor = rgbToHex(profileData.ledRed, profileData.ledGreen, profileData.ledBlue);
            try {
                $("#color-picker").spectrum("set", hexColor);
            } catch (error) {
                console.warn('Impossible d\'appliquer la couleur LED:', error);
            }
        }

        // 9. Debug (si présent)
        if (profileData.debugModeEnabled !== undefined)
            setElementValue("debugMode", profileData.debugModeEnabled, 'checkbox');

        // 10. Synchronisation UI globale
        if (typeof updateLedDot === 'function') updateLedDot();
        if (typeof updateVisibility === 'function') updateVisibility();
        if (typeof reapplyTemperatureLimits === 'function') reapplyTemperatureLimits();

        // 11. Heatmap saisonnière (si présente et mode actif)
        if (
            profileData.seasonalModeEnabled &&
            typeof window.seasonalData !== "undefined" &&
            Array.isArray(window.seasonalData) &&
            typeof createHeatmap === "function"
        ) {
            createHeatmap(window.seasonalData);
            // Sélection du jour courant pour édition, si besoin :
            if (typeof updateSeasonalVisibility === "function") updateSeasonalVisibility();
        }

        // 12. Log
        if (window.debugMode)
            console.log('✅ Profil complet appliqué à l\'interface utilisateur', profileData);
    } catch (e) {
        alert('❌ Erreur application du profil : ' + e.message);
        if (window.debugMode) console.error(e);
    }
}

// ✅ Charger un profil par nom (utilisé dans la liste)
async function loadNamedProfile(name) {
    let previousCursor = document.body.style.cursor;
    try {
        // Désactiver l'UI pendant le chargement
        document.body.style.cursor = "wait";
        const notif = document.createElement('div');
        notif.textContent = "⏳ Chargement du profil...";
        notif.id = "profile-loading-notif";
        notif.style = "position:fixed;top:10px;right:10px;z-index:1000;background:#222;color:#fff;padding:10px;border-radius:6px";
        document.body.appendChild(notif);

        // Charger le profil depuis la nouvelle structure
        const response = await fetch(`/loadProfile?name=${encodeURIComponent(name)}`);
        if (!response.ok) {
            throw new Error('Profil non trouvé ou erreur serveur');
        }

        const profile = await response.json();

        // Appliquer le profil à l'interface
        await applyCompleteProfileToUI(profile);

        // Charger les données saisonnières si disponibles
        if (profile.seasonalModeEnabled) {
            try {
                const seasonalResponse = await fetch(`/getProfileSchedule?name=${encodeURIComponent(name)}`);
                if (seasonalResponse.ok) {
                    const seasonalData = await seasonalResponse.json();
                    if (Array.isArray(seasonalData) && seasonalData.length === 366) {
                        window.seasonalData = seasonalData;
                        if (typeof createHeatmap === 'function') {
                            createHeatmap(seasonalData);
                        }
                        console.log('✅ Données saisonnières chargées');
                    }
                }
            } catch (seasonalError) {
                console.warn('⚠️ Impossible de charger les données saisonnières:', seasonalError);
            }
        }

        // Scroll vers la section appropriée
        if (profile.seasonalModeEnabled && document.getElementById('seasonalHeatmap')) {
            document.getElementById('seasonalHeatmap').scrollIntoView({ behavior: 'smooth', block: 'center' });
        } else if (document.getElementById('configTempChart')) {
            document.getElementById('configTempChart').scrollIntoView({ behavior: 'smooth', block: 'center' });
        }

        alert('✅ Profil chargé : ' + name);
        if (window.debugMode) console.log("✅ Profil chargé :", profile);

    } catch (error) {
        alert('❌ Erreur lors du chargement : ' + error.message);
        console.error("Erreur chargement profil:", error);
    } finally {
        // Restaurer l'UI
        document.body.style.cursor = previousCursor;
        const notif = document.getElementById('profile-loading-notif');
        if (notif) notif.remove();
    }
}

// Helper pour fixer la valeur d’un élément DOM
function setElementValue(elementId, value, type = 'input') {
    const element = document.getElementById(elementId);
    if (!element) {
        if (window.debugMode) console.warn(`Élément ${elementId} non trouvé`);
        return;
    }
    switch (type) {
        case 'checkbox':
            element.checked = Boolean(value);
            break;
        case 'text':
            element.textContent = value;
            break;
        default:
            element.value = value;
            break;
    }
}

// Helper hex pour la couleur LED
function rgbToHex(r, g, b) {
    return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}


// ✅ Fonction helper pour définir la valeur d'un élément
function setElementValue(elementId, value, type = 'input') {
    const element = document.getElementById(elementId);
    if (!element) {
        console.warn(`Élément ${elementId} non trouvé`);
        return;
    }
    
    switch (type) {
        case 'checkbox':
            element.checked = Boolean(value);
            break;
        case 'text':
            element.textContent = value;
            break;
        default:
            element.value = value;
            break;
    }
}

// ✅ Supprimer un profil
async function deleteProfile(name) {
    if (!confirm(`Supprimer le profil "${name}" et toutes ses données ?`)) return;
    
    try {
        const response = await fetch(`/deleteProfile?name=${encodeURIComponent(name)}`, {
            method: 'DELETE'
        });
        
        if (response.ok) {
            alert('✅ Profil supprimé avec succès');
            refreshProfileList();
        } else {
            const error = await response.text();
            alert('❌ Erreur lors de la suppression : ' + error);
        }
    } catch (error) {
        alert('❌ Erreur : ' + error.message);
    }
}

// ✅ Renommer un profil avec la nouvelle structure
async function renameProfile(oldName) {
    let newName = prompt("Nouveau nom :", oldName);
    if (!newName || newName === oldName) return;
    
    if (!/^[\w\d _-]+$/.test(newName)) {
        alert('⛔ Nom invalide (lettres, chiffres, espace, - et _ autorisés)');
        return;
    }

    try {
        const response = await fetch(`/renameProfile?from=${encodeURIComponent(oldName)}&to=${encodeURIComponent(newName)}`, {
            method: 'POST'
        });
        
        if (response.ok) {
            alert('✅ Profil renommé avec succès');
            refreshProfileList();
        } else {
            const error = await response.text();
            alert('❌ Erreur lors du renommage : ' + error);
        }
    } catch (error) {
        alert('❌ Erreur : ' + error.message);
    }
}


// ✅ Charger un profil via prompt
async function loadProfile() {
    try {
        const res = await fetch('/listProfiles');
        if (!res.ok) throw new Error('Erreur réseau');
        const profiles = await res.json();

        if (!profiles.length) {
            alert('❌ Aucun profil personnalisé trouvé');
            return;
        }

        const name = prompt("Choisir un profil à charger :\n" + profiles.join('\n'));
        if (!name || !profiles.includes(name)) {
            alert('⛔ Profil invalide ou annulé');
            return;
        }

        await loadNamedProfile(name);

    } catch (err) {
        alert('❌ Erreur : ' + err.message);
    }
}

// Importer un fichier JSON depuis l'utilisateur
document.getElementById('profileUpload').addEventListener('change', async (event) => {
	const file = event.target.files[0];
	if (!file) return;

	const reader = new FileReader();
	reader.onload = async function (e) {
		try {
			const json = JSON.parse(e.target.result);
			if (!json.name || !json.temperatures) throw new Error("Format invalide");

			const response = await fetch('/uploadProfile', {
				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify(json)
			});
			if (response.ok) {
				alert("✅ Profil importé");
				refreshProfileList();
			} else {
				alert("❌ Erreur lors de l'importation");
			}
		} catch (e) {
			alert("❌ Fichier invalide : " + e.message);
		}
	};
	reader.readAsText(file);
});

// Dans configuration.js - remplacer loadFromDevice() par:
async function loadFromDevice() {
    if (!chart) {
        console.warn("⏳ Chart non encore prêt, tentative retardée...");
        setTimeout(loadFromDevice, 400);
        return;
    }

    try {
        // 1. Charger la courbe de température
        const response = await fetch('/getTempCurve');
        if (response.ok) {
            const data = await response.json();
            temperatureData = data;
            extendedTemperatureData = [
                temperatureData[temperatureData.length - 1],
                ...temperatureData,
                temperatureData[0]
            ];
            chart.data.datasets[0].data = extendedTemperatureData;
            chart.update();
            updateTempGrid();
            updateStatus();
            reapplyTemperatureLimits();
        }
        
               
    } catch (error) {
        console.error("❌ Erreur lors du chargement des données :", error);
    }
}

// Fonction pour mettre à jour l'heure actuelle
function updateCurrentHour() {
    const newHour = new Date().getHours();
    if (newHour !== currentHour) {
        currentHour = newHour;
        updateTempGrid();
        updateStatus();
        console.log(`🕐 Heure mise à jour: ${currentHour}h`);
    }
}

// Fonction pour valider les coordonnées
function validateCoordinates(lat, lon) {
  const latitude = parseFloat(lat);
  const longitude = parseFloat(lon);
  
  if (isNaN(latitude) || isNaN(longitude)) {
    return { valid: false, message: "Coordonnées invalides" };
  }
  
  if (latitude < -90 || latitude > 90) {
    return { valid: false, message: "Latitude doit être entre -90 et 90" };
  }
  
  if (longitude < -180 || longitude > 180) {
    return { valid: false, message: "Longitude doit être entre -180 et 180" };
  }
  
  return { valid: true };
}

function reapplyTemperatureLimits() {
    const useLimit = document.getElementById("useLimitTemp").checked;
    if (!useLimit) return;

    const min = parseFloat(document.getElementById("minTempSet").value);
    const max = parseFloat(document.getElementById("maxTempSet").value);

    temperatureData = temperatureData.map(temp =>
        Math.min(max, Math.max(min, temp))
    );

    updateChartAndGrid();
}
// Fonction pour mettre à jour les valeurs globales
function updateGlobalTemps() {
    const minTempInput = document.getElementById("minTempSet");
    const maxTempInput = document.getElementById("maxTempSet");

    globalMinTempSet = parseFloat(minTempInput.value) || 0;
    globalMaxTempSet = parseFloat(maxTempInput.value) || 40;

    console.log(`Températures mises à jour : Min = ${globalMinTempSet}, Max = ${globalMaxTempSet}`);
}

function updateStatus() {
}

function saveConfigurationled() {
	const state = ledToggle.checked;
	const brightness = brightnessSlider.value;
	const color = $("#color-picker").spectrum("get").toHexString();

	// Conversion de la couleur hexadécimale en composantes RGB
	const red = parseInt(color.substr(1, 2), 16);
	const green = parseInt(color.substr(3, 2), 16);
	const blue = parseInt(color.substr(5, 2), 16);

	// Envoi des paramètres au serveur
	fetch(`/updateLed?state=${state}&brightness=${brightness}&red=${red}&green=${green}&blue=${blue}`)
		.then(response => response.text())
		.then(data => console.log(data))
		.catch(error => console.error('Erreur:', error));
}

function saveTemperatureBinOptimized() {
    console.log('🔧 Génération temperature.bin optimisé (int16)...');
    
    if (!seasonalData || seasonalData.length !== 366) {
        alert('❌ Données saisonnières manquantes');
        return;
    }
    
    // Buffer int16: 366 × 24 × 2 = 17,568 bytes
    const buffer = new ArrayBuffer(366 * 24 * 2);
    const view = new DataView(buffer);
    
    let totalTemps = 0;
    let minTemp = Infinity, maxTemp = -Infinity;
    
    for (let day = 0; day < 366; day++) {
        const dayData = seasonalData[day] || Array(24).fill(22.0);
        
        for (let hour = 0; hour < 24; hour++) {
            const offset = (day * 24 + hour) * 2;
            let temp = dayData[hour] || 22.0;
            
            // Validation température
            if (temp < -100 || temp > 100) {
                console.warn(`⚠️ Température extrême jour ${day+1}h${hour}: ${temp}°C`);
                temp = Math.max(-100, Math.min(100, temp));
            }
            
            // Conversion float → int16 (1 décimale)
            const tempInt16 = Math.round(temp * 10);
            view.setInt16(offset, tempInt16, true); // little-endian
            
            totalTemps++;
            minTemp = Math.min(minTemp, temp);
            maxTemp = Math.max(maxTemp, temp);
        }
    }
    
    console.log(`📊 Fichier int16 généré:`);
    console.log(`   - Températures: ${totalTemps}`);
    console.log(`   - Plage: ${minTemp.toFixed(1)}°C à ${maxTemp.toFixed(1)}°C`);
    console.log(`   - Taille: ${buffer.byteLength} bytes (économie: ${35136 - buffer.byteLength} bytes)`);
    
    // Télécharger
    const blob = new Blob([buffer], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'temperature_int16.bin';
    a.click();
    URL.revokeObjectURL(url);
    
    showNotification(`✅ temperature_int16.bin sauvegardé (${(buffer.byteLength/1024).toFixed(1)} KB)`, 'success');
}