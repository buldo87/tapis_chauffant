// js/configuration.js

/**
 * Module de configuration - Gestion des paramètres de l'interface
 */

 // coube ajustable
let chart;
let temperatureData = Array(24).fill(0).map((_, i) => 22 + Math.sin(i / 24 * Math.PI * 2) * 6); // Courbe sinusoïdale par défaut
let extendedTemperatureData = [temperatureData[temperatureData.length - 1], ...temperatureData, temperatureData[0]];
let isDragging = false;
let currentHour = new Date().getHours();
let currentMin = new Date().getMinutes();
// Camera functions
function updateCameraVisibility() {
	const show = document.getElementById("showCamera").checked;
	const container = document.getElementById("cameraContainer");
	const img = document.getElementById("camStream");

	container.style.display = show ? "block" : "none";
	cameraEnabled = show;

	fetch("/setCamera?enabled=" + (show ? 1 : 0))
		.catch(err => console.error("Erreur setCamera:", err));

	if (show) {
		refreshInterval = setInterval(() => {
			img.src = "/capture?ts=" + new Date().getTime();
		}, 1000);
	} else {
		clearInterval(refreshInterval);
	}
}

// Save all settings
function applyAllSettings() {
    // Liste des éléments requis avec vérification d'existence
    const getElement = (id) => {
        const el = document.getElementById(id);
        if (!el) console.error(`Élément ${id} non trouvé`);
        return el;
    };

    const elements = {
        hysteresisSet: document.getElementById("hysteresisSet"),
        KpSet: document.getElementById("KpSet"),
        KiSet: document.getElementById("KiSet"),
        KdSet: document.getElementById("KdSet"),
        usePWM: document.getElementById("usePWM"),
        latInput: document.getElementById("latInput"),
        lonInput: document.getElementById("lonInput"),
        weatherMode: document.getElementById("weatherMode"),
        showCamera: document.getElementById("showCamera"),
        useLimitTemp: document.getElementById("useLimitTemp"),
        MaxTempSet: document.getElementById("MaxTempSet"),
        MinTempSet: document.getElementById("MinTempSet")
    };
	    // Vérifier les éléments manquants
    for (const [key, element] of Object.entries(elements)) {
        if (!element) {
            console.error(`Élément ${key} non trouvé dans le DOM`);
            return;
        }
    }

        // Récupérer les valeurs seulement si les éléments existent
    const hysteresis = elements.hysteresisSet.value;
    const Kp = elements.KpSet.value;
    const Ki = elements.KiSet.value;
    const Kd = elements.KdSet.value;
    const usePWM = elements.usePWM.checked ? 1 : 0;
    const lat = elements.latInput.value;
    const lon = elements.lonInput.value;
    const weatherEnabled = elements.weatherMode.checked ? 1 : 0;
    const showCamera = elements.showCamera.checked ? 1 : 0;
    const useLimitTemp = elements.useLimitTemp.checked ? 1 : 0;
    globalMaxTempSet = elements.MaxTempSet.value;
    globalMinTempSet = elements.MinTempSet.value;

	Promise.all([
        fetch("/setGlobalMaxTemp?globalMaxTempSet="+globalMaxTempSet),
        fetch("/setGlobalMinTempSet?globalMinTempSet="+globalMinTempSet),
		fetch("/setHysteresis?hysteresis="+hysteresis),
		fetch("/setPID?Kp="+Kp+"&Ki="+Ki+"&Kd="+Kd),
		fetch("/setPWMMode?usePWM="+usePWM),
        fetch("/setLimitTemp?useLimitTemp="+useLimitTemp),
		fetch("/setWeather?lat="+lat+"&lon="+lon+"&enabled="+weatherEnabled),
		fetch("/setCamera?enabled="+showCamera),
        fetch("/setWeatherEnabled="+weatherEnabled)
	])
	.then(() => {
		// Success animation
		const btn = document.getElementById("applyBtn");
		btn.innerHTML = '<i class="fas fa-check mr-2"></i>Sauvegardé!';
		btn.style.background = '#4ade80';
		setTimeout(() => {
			btn.innerHTML = '<i class="fas fa-download mr-2"></i>Sauvegarder Configuration';
			btn.style.background = '';
		}, 2000);
	})
	.catch(error => {
		console.error("Erreur:", error);
		alert("Erreur lors de la sauvegarde des paramètres: " + error.message);
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
const chartConfig = {
    type: 'line',
    data: {
        //labels: Array.from({length: 24}, (_, i) => i + 'h'),
        labels: ['23h', '0h', '1h', '2h', '3h', '4h', '5h', '6h', '7h', '8h', '9h', '10h', '11h',
                 '12h', '13h', '14h', '15h', '16h', '17h', '18h', '19h', '20h', '21h', '22h', '23h', '0h'],
     
        datasets: [{
            label: 'Température (°C)',
            //data: temperatureData,
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
                // Valeur minimale de l'axe Y basée sur les données, moins 1
                return Math.min(...context.chart.data.datasets[0].data) - 1;
            },
                max: function(context) {
                    // Valeur maximale de l'axe Y basée sur les données, plus 1
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
                    const dataY = chart.scales.y.getValueForPixel(canvasPosition.y);
                    const newTemp = Math.max(globalMinTempSet, Math.min(globalMaxTempSet, dataY));
                    updateTemperatureFromExtendedIndex(index, newTemp);
                }

            }
        }
    }
};

// Initialisation du graphique
function initChart() {
    const ctx = document.getElementById('temperatureChart').getContext('2d');
    
    // Vérifier que l'élément canvas existe
    if (!ctx) {
        console.error("Élément canvas 'temperatureChart' non trouvé");
        return;
    }
    extendedTemperatureData = [temperatureData[temperatureData.length - 1], ...temperatureData, temperatureData[0]];
    chart = new Chart(ctx, chartConfig);

    // Vérifier que le graphique a été créé avec succès
    if (!chart) {
        console.error("Erreur lors de la création du graphique Chart.js");
        return;
    }

    // Gestion du drag pour modifier les températures
    ctx.canvas.addEventListener('mousedown', startDrag);
    ctx.canvas.addEventListener('mousemove', drag);
    ctx.canvas.addEventListener('mouseup', endDrag);
    ctx.canvas.addEventListener('mouseleave', endDrag);

    // Gestion tactile pour les appareils mobiles
    ctx.canvas.addEventListener('touchstart', handleTouchStart, { passive: false });
    ctx.canvas.addEventListener('touchmove', handleTouchMove, { passive: false });
    ctx.canvas.addEventListener('touchend', handleTouchEnd, { passive: false });

    // Initialiser l'affichage
    updateTempGrid();
    updateStatus();
    
    // Exposer les variables globalement après initialisation pour les autres modules
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
    const grid = document.getElementById('tempGrid');

    if (!grid) {
        console.warn("⚠️ Élément 'tempGrid' non trouvé");
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

// Fonction updateStatus
function updateStatus() {
    // Mettre à jour l'heure actuelle
    const currentHourElement = document.getElementById('currentHour');
    if (currentHourElement) {
        currentHourElement.textContent = currentHour + 'h';
    }
    
    // Mettre à jour la température cible
    const targetTempElement = document.getElementById('targetTemp');
    if (targetTempElement && temperatureData[currentHour] !== undefined) {
        targetTempElement.textContent = temperatureData[currentHour].toFixed(1) + '°C';
    }
    
    // Mettre à jour les statistiques de la courbe
    const minTemp = Math.min(...temperatureData);
    const maxTemp = Math.max(...temperatureData);
    const avgTemp = temperatureData.reduce((sum, temp) => sum + temp, 0) / temperatureData.length;
    
    console.log(`📊 Stats courbe - Min: ${minTemp.toFixed(1)}°C, Max: ${maxTemp.toFixed(1)}°C, Moy: ${avgTemp.toFixed(1)}°C`);
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
            updateChartAndGrid();
            break;
        case 'foret':
            temperatureData = Array(24).fill(0).map((_, i) => {
                if (i >= 6 && i < 10) return 24;
                if (i >= 10 && i < 16) return 28;
                if (i >= 16 && i < 22) return 25;
                return 23;
            });
            updateChartAndGrid();
            break;
        case 'tropical':
            temperatureData = Array(24).fill(0).map((_, i) => {
                return 28 + Math.sin((i - 8) / 24 * Math.PI * 2) * 4;
            });
            updateChartAndGrid();
            break;
        case 'desert':
            temperatureData = Array(24).fill(0).map((_, i) => {
                if (i >= 6 && i < 9) return 27;
                if (i >= 9 && i < 17) return 32;
                if (i >= 17 && i < 23) return 28;
                return 23;
            });
            updateChartAndGrid();
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
            updateChartAndGrid();
            break;
        default:
            // Mise à jour pour tous les autres cas
            updateChartAndGrid();
            break;
    }
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
    indicator.innerHTML = '<div style="text-align: center; padding: 20px; color: #fff;">🌡️ Récupération des données météo...</div>';
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
	chart.data.datasets[0].data = [...temperatureData];
	chart.update();
	updateTempGrid();
}

function resetToDefault() {
	temperatureData = Array(24).fill(0).map((_, i) => 24 + Math.sin(i-6 / 24 * Math.PI * 2) * 6);
	chart.data.datasets[0].data = [...temperatureData];
	chart.update();
	updateTempGrid();
	updateStatus();
}

function saveProfile() {
	const profile = {
		name: prompt('Nom du profil:', 'Mon Profil'),
		temperatures: [...temperatureData],
		timestamp: new Date().toISOString()
	};
}

function loadProfile() {
	loadFromDevice();
}

async function sendToDevice() {
    try {
        // Envoyer chaque point de température
        for (let hour = 0; hour < temperatureData.length; hour++) {
            const response = await fetch(`/setTemp?hour=${hour}&temp=${temperatureData[hour]}`);
            if (!response.ok) {
                throw new Error(`Erreur HTTP: ${response.status}`);
            }
        }
        
        // Activer le mode courbe de température
        await fetch('/setTempCurveMode?enabled=1');
        
        // Feedback visuel
        const btn = document.getElementById('sendToDeviceBtn');
        if (btn) {
            btn.textContent = '✅ Envoyé !';
            setTimeout(() => {
                btn.textContent = '📡 Envoyer au Tapis';
            }, 2000);
        }
    } catch (error) {
        console.error('Erreur:', error);
        alert('Erreur lors de l\'envoi: ' + error.message);
    }
}

async function sendToDevice2() {
	try {
		// Envoyer les données de température
		const response = await fetch('/setTempCurve', {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify(temperatureData)
		});
		
		if (response.ok) {
			// Activer le mode courbe de température
			await fetch('/setTempCurveMode?enabled=1');
			
			const btn = event.target;
			const originalText = btn.textContent;
			btn.textContent = '✅ Envoyé !';
			btn.style.background = 'linear-gradient(45deg, #2ecc71, #27ae60)';
			
			setTimeout(() => {
				btn.textContent = originalText;
				btn.style.background = '';
			}, 2000);
		} else {
			const error = await response.text();
			alert('Erreur: ' + error);
		}
	} catch (error) {
		alert('Erreur de connexion: ' + error.message);
	}
}

async function loadFromDevice() {
	try {
		const response = await fetch('/getTempCurve');
		if (response.ok) {
			const data = await response.json();
			temperatureData = data;
			chart.data.datasets[0].data = [...temperatureData];
			chart.update();
			updateTempGrid();
			updateStatus();
			alert('✅ Configuration chargée depuis le tapis chauffant !');
		}
	} catch (error) {
		alert('Erreur de chargement: ' + error.message);
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

// ========================================
// 5. MISE À JOUR DE updateVisibility()
// ========================================

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

// Fonction pour mettre à jour les valeurs globales
function updateGlobalTemps() {
    const minTempInput = document.getElementById("minTempSet");
    const maxTempInput = document.getElementById("maxTempSet");

    globalMinTempSet = parseFloat(minTempInput.value) || 0;
    globalMaxTempSet = parseFloat(maxTempInput.value) || 40;

    console.log(`Températures mises à jour : Min = ${globalMinTempSet}, Max = ${globalMaxTempSet}`);
}

// Initialisation
window.addEventListener('load', function() {
	initChart();
	loadFromDevice(); // Charger les données existantes
	setInterval(updateCurrentHour, 60000);
	setInterval(updateStatus, 10000);
});