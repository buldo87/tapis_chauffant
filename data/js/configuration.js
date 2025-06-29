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
			// Remplacer l’image visible seulement après chargement
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
        "weatherMode", "showCamera", "cameraResolution", "useLimitTemp",
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
        cameraEnabled: get("showCamera").checked ? 1 : 0,
		cameraResolution: get("cameraResolution").value,
        useLimitTemp: get("useLimitTemp").checked ? 1 : 0,
        tempCurve: [...temperatureData],  
        ledState: get("led-toggle").checked,
        ledBrightness: parseInt(get("brightness-slider").value),
        ledRed: red,
        ledGreen: green,
        ledBlue: blue
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
    const canvas = document.getElementById('temperatureChart');
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
                    text: 'Courbe de température sur 24h - Cliquez et glissez pour ajuster',
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

    // 🔁 Initialiser l’affichage
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
    updateTempGrid();
    updateStatus();

    // Exposer la mise à jour globalement
    window.temperatureData = temperatureData;
    window.extendedTemperatureData = extendedTemperatureData;
}

// Fonction updateTempGrid
function updateTempGrid() {
    const grid = document.getElementById('temperatureChart');

    if (!grid) {
        console.warn("⚠️ Élément 'temperatureChart' non trouvé");
        return;
    }

    // Vider le contenu existant
    grid.innerHTML = '';

    // Utiliser extendedTemperatureData si vous avez 26 heures
    extendedTemperatureData.forEach((temp, hour) => {
        const hourLabel = hour === 0 ? '23h' : hour <= 24 ? `${hour - 1}h` : '0h';
        const hourControl = document.createElement('div');
        hourControl.className = `hour-control ${hour === currentHour + 1 ? 'current-hour' : ''}`;
        hourControl.innerHTML = `
            <div class="hour-label">${hourLabel}</div>
            <div class="hour-temp">${temp.toFixed(1)}°C</div>
        `;

        // Gestionnaire de clic pour modification manuelle
        hourControl.onclick = () => {
            const displayHour = hour === 0 ? '23h' : hour <= 24 ? `${hour - 1}h` : '0h';
            const newTemp = prompt(`Température pour ${displayHour} (${globalMinTempSet}-${globalMaxTempSet}°C):`, temp.toFixed(1));
            if (newTemp && !isNaN(newTemp)) {
                const validTemp = Math.max(globalMinTempSet, Math.min(globalMaxTempSet, parseFloat(newTemp)));
                updateTemperature(hour, validTemp);

                if (validTemp !== parseFloat(newTemp)) {
                    alert(`Température ajustée à ${validTemp}°C (limite de sécurité ${globalMinTempSet}-${globalMaxTempSet}°C)`);
                }
            }
        };

        grid.appendChild(hourControl);
    });
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
        updateTempGrid();
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
// ✅ Sauvegarder un nouveau profil
function saveProfile() {
    let name = prompt('Nom du profil :', 'Profil');
    if (!name || !/^[\w\d _-]+$/.test(name)) {
        alert('⛔ Nom invalide (lettres, chiffres, espace, - et _ autorisés)');
        return;
    }

    const profile = {
        name,
        temperatures: [...temperatureData],
        timestamp: new Date().toISOString()
    };

    fetch('/saveProfile', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(profile)
    })
    
    .then(res => res.text())
    .then(msg => alert('✅ ' + msg))
    .catch(err => alert('❌ ' + err.message));

    refreshProfileList();
}

// ✅ Rafraîchir la liste HTML des profils dans la page
async function refreshProfileList() {
    const ul = document.getElementById('profileList');
    ul.innerHTML = '<li>Chargement...</li>';

    try {
        const res = await fetch('/listProfiles');
        const profiles = await res.json();

        ul.innerHTML = '';
        profiles.forEach(name => {
            const li = document.createElement('li');
            li.classList.add('flex', 'justify-between', 'items-center', 'border-b', 'pb-1');

            li.innerHTML = `
                <span>${name}</span>
                <div class="flex space-x-1">
                    <button onclick="loadNamedProfile('${name}')" title="Charger">📂</button>
                    <button onclick="renameProfile('${name}')" title="Renommer">✏️</button>
                    <button onclick="deleteProfile('${name}')" title="Supprimer">🗑️</button>
                    <a href="/downloadProfile?name=${encodeURIComponent(name)}" download title="Télécharger">⬇️</a>
                </div>
            `;
            ul.appendChild(li);
        });
    } catch (err) {
        ul.innerHTML = '<li class="text-red-500">Erreur de chargement</li>';
        console.error(err);
    }
}
// ✅ Charger un profil par nom (utilisé dans la liste)
async function loadNamedProfile(name) {
    try {
        const url = '/loadNamedProfile?name=' + encodeURIComponent(name);
        //console.log("[DEBUG JS] Requête vers :", url);

        const res = await fetch(url);
        //console.log("[DEBUG JS] Statut réponse :", res.status);

        if (!res.ok) throw new Error('Fichier non trouvé');

        const profile = await res.json();
        //console.log("[DEBUG JS] Contenu JSON :", profile);

        temperatureData = profile.temperatures;
        updateChartAndGrid();
        reapplyTemperatureLimits();
        alert('✅ Profil chargé : ' + name);
    } catch (e) {
        alert('❌ ' + e.message);
        //console.error("[DEBUG JS] Erreur:", e);
    }
}
// ✅ Supprimer un profil
async function deleteProfile(name) {
    if (!confirm(`Supprimer le profil "${name}" ?`)) return;
    await fetch(`/deleteProfile?name=${encodeURIComponent(name)}`);
    refreshProfileList();
}
// ✅ Renommer un profil
async function renameProfile(oldName) {
    let newName = prompt("Nouveau nom :", oldName.replace('.json', ''));
    if (!newName || newName === oldName) return;
    if (!newName || !/^[\w\d _-]+$/.test(newName)) {
        alert('⛔ Nom invalide (lettres, chiffres, espace, - et _ autorisés)');
        return;
    }

    const res = await fetch(`/renameProfile?from=${encodeURIComponent(oldName)}&to=${encodeURIComponent(newName)}`);
    if (res.ok) {
        alert('✅ Profil renommé');
        refreshProfileList();
    } else {
        alert('❌ Erreur lors du renommage');
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

        const fileRes = await fetch('/' + encodeURIComponent(name));
        if (!fileRes.ok) throw new Error('Fichier introuvable');
        const profile = await fileRes.json();

        temperatureData = profile.temperatures;
        updateChartAndGrid();
        alert('✅ Profil chargé : ' + name);

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
//  MISE À JOUR DE updateVisibility()
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

    if (weatherChecked) {
        // Auto-détection de la position si les champs sont vides
        const lat = document.getElementById('latInput').value;
        const lon = document.getElementById('lonInput').value;
        if (!lat || !lon) {
			// Position par défaut (Paris)
			Lat = 48.8566;
			Lon = 2.3522;
            //getCurrentLocation();
        }
    }

    // Temperature limit settings
    const limitTempChecked = document.getElementById("useLimitTemp").checked;
    const limitTempDiv = document.getElementById("limitTempSettings");

    if (limitTempDiv) limitTempDiv.style.display = limitTempChecked ? "block" : "none";
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



