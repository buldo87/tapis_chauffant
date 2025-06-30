// seasonal.js - Gestion du suivi saisonnier automatique
let currentSelectedDay = null;
let seasonalData = null;
let isSeasonalMode = false;
let seasonalInitialized = false;

// Configuration de la heatmap améliorée
const HEATMAP_CONFIG = {
    cellSize: 10,  // Augmenté pour une meilleure visibilité
    cellPadding: 1,
    monthLabels: ['Jan', 'Fév', 'Mar', 'Avr', 'Mai', 'Jun', 'Jul', 'Aoû', 'Sep', 'Oct', 'Nov', 'Déc'],
    dayLabels: ['D', 'L', 'M', 'M', 'J', 'V', 'S']
};

// Initialisation du système saisonnier
function initSeasonalSystem() {
    if (seasonalInitialized) return;
    
    console.log('🌍 Initialisation du système saisonnier...');
    
    // Initialiser les événements
    initSeasonalEventListeners();
    
    // Créer la heatmap container
    createHeatmapContainer();
    
    // Vérifier l'état initial du mode saisonnier
    updateSeasonalVisibility();
    
    seasonalInitialized = true;
    console.log('✅ Système saisonnier initialisé');
}

// Nouvelle fonction pour gérer la visibilité des éléments saisonniers
function updateSeasonalVisibility() {
    const seasonalMode = document.getElementById('seasonalMode');
    const seasonalNavigation = document.getElementById('seasonalNavigation');
    const heatmapContainer = document.getElementById('seasonalHeatmap');
    const toolsContainer = document.getElementById('seasonalTools');
    
    if (!seasonalMode) return;
    
    const isEnabled = seasonalMode.checked;
    
    // Afficher/masquer la navigation saisonnière
    if (seasonalNavigation) {
        seasonalNavigation.style.display = isEnabled ? 'block' : 'none';
    }
    
    if (heatmapContainer) {
        heatmapContainer.style.display = isEnabled ? 'block' : 'none';
    }
    
    if (toolsContainer) {
        toolsContainer.style.display = isEnabled ? 'block' : 'none';
    }
    
    // Si le mode saisonnier est activé
    if (isEnabled) {
        // Créer des données de démonstration si nécessaire
        if (!seasonalData) {
            createDemoSeasonalData();
        }
        
        // ✅ SÉLECTIONNER AUTOMATIQUEMENT LE JOUR ACTUEL - CORRIGÉ POUR 2025
        const today = new Date();
        const startOfYear = new Date(today.getFullYear(), 0, 1);
        const dayOfYear = Math.floor((today - startOfYear) / (1000 * 60 * 60 * 24));
        
        // Sélectionner le jour actuel après un petit délai pour laisser la heatmap se créer
        setTimeout(() => {
            selectDay(dayOfYear);
            console.log(`📅 Jour actuel sélectionné automatiquement: ${dayOfYear + 1} (${today.toLocaleDateString('fr-FR')})`);
        }, 500);
    } else {
        // Réinitialiser la sélection quand le mode est désactivé
        currentSelectedDay = null;
    }
    
    console.log(`🌍 Mode saisonnier ${isEnabled ? 'activé' : 'désactivé'}`);
}

// Initialisation des données saisonnières (processus long)
async function initializeSeasonalData() {
    const initButton = document.getElementById('initSeasonalBtn');
    const progressDiv = document.getElementById('seasonalProgress');
    
    if (!initButton || !progressDiv) return;
    
    // Vérifier les coordonnées
    const lat = document.getElementById('latInput').value;
    const lon = document.getElementById('lonInput').value;
    
    if (!lat || !lon) {
        alert('⚠️ Veuillez configurer les coordonnées GPS avant d\'initialiser les données saisonnières');
        return;
    }
    
    // Confirmation de l'utilisateur
    const confirmed = confirm(
        '🌍 Initialisation des données saisonnières\n\n' +
        'Cette opération va télécharger 4 années de données météorologiques historiques.\n' +
        'Cela peut prendre plusieurs minutes.\n\n' +
        'Voulez-vous continuer ?'
    );
    
    if (!confirmed) return;
    
    try {
        // Désactiver le bouton et afficher le progrès
        initButton.disabled = true;
        initButton.textContent = '⏳ Initialisation en cours...';
        progressDiv.style.display = 'block';
        
        // Simuler le processus d'initialisation pour l'instant
        await simulateInitialization();
        
        // Créer des données de démonstration
        createDemoSeasonalData();
        
        showNotification('✅ Données saisonnières initialisées avec succès !', 'success');
        
    } catch (error) {
        console.error('❌ Erreur lors de l\'initialisation:', error);
        showNotification('❌ Erreur lors de l\'initialisation: ' + error.message, 'error');
    } finally {
        initButton.disabled = false;
        initButton.textContent = '📥 Initialiser les données saisonnières';
        progressDiv.style.display = 'none';
    }
}

// Simulation du processus d'initialisation
async function simulateInitialization() {
    const progressBar = document.getElementById('seasonalProgressBar');
    const progressText = document.getElementById('seasonalProgressText');
    
    if (!progressBar || !progressText) return;
    
    const steps = [
        { progress: 10, text: 'Connexion à l\'API météo...' },
        { progress: 25, text: 'Téléchargement année 2021...' },
        { progress: 40, text: 'Téléchargement année 2022...' },
        { progress: 55, text: 'Téléchargement année 2023...' },
        { progress: 70, text: 'Téléchargement année 2024...' },
        { progress: 85, text: 'Calcul des moyennes...' },
        { progress: 95, text: 'Sauvegarde des données...' },
        { progress: 100, text: 'Initialisation terminée !' }
    ];
    
    for (const step of steps) {
        progressBar.style.width = `${step.progress}%`;
        progressText.textContent = step.text;
        await new Promise(resolve => setTimeout(resolve, 800));
    }
}

// Création de données de démonstration - CORRIGÉE POUR 2025
function createDemoSeasonalData() {
    // Générer 366 jours de données de température (24 heures par jour) pour 2025
    seasonalData = [];
    
    for (let day = 0; day < 366; day++) {
        const dayTemps = [];
        
        // Simuler une variation saisonnière
        const seasonalBase = 20 + 10 * Math.sin((day / 366) * 2 * Math.PI - Math.PI/2);
        
        for (let hour = 0; hour < 24; hour++) {
            // Variation journalière
            const dailyVariation = 5 * Math.sin((hour / 24) * 2 * Math.PI - Math.PI/2);
            
            // Ajouter un peu de bruit
            const noise = (Math.random() - 0.5) * 2;
            
            const temp = seasonalBase + dailyVariation + noise;
            dayTemps.push(Math.round(temp * 10) / 10);
        }
        
        seasonalData.push(dayTemps);
    }
    
    // Afficher la heatmap si le mode saisonnier est activé
    const seasonalMode = document.getElementById('seasonalMode');
    if (seasonalMode && seasonalMode.checked) {
        createHeatmap(seasonalData);
        document.getElementById('seasonalHeatmap').style.display = 'block';
    }
}

// Création de la heatmap calendaire - Largeur complète avec tooltip corrigé
function createHeatmap(data) {
    const container = document.getElementById('heatmapCanvas');
    if (!container) return;
    
    // Nettoyer le conteneur
    container.innerHTML = '';
    
    // Créer le canvas pour la heatmap - Largeur complète
    const canvas = document.createElement('canvas');
    const containerWidth = container.offsetWidth || 1200;
    canvas.width = Math.max(containerWidth - 40, 1000);  // Largeur adaptative
    canvas.height = 180;  // Hauteur optimisée
    canvas.style.width = '100%';
    canvas.style.height = 'auto';
    canvas.style.cursor = 'pointer';
    canvas.style.display = 'block';
    
    const ctx = canvas.getContext('2d');
    
    // Dessiner la heatmap
    drawYearlyHeatmap(ctx, data, canvas.width, canvas.height);
    
    // Ajouter les événements de clic et survol
    canvas.addEventListener('click', (event) => {
        const rect = canvas.getBoundingClientRect();
        const x = (event.clientX - rect.left) * (canvas.width / rect.width);
        const y = (event.clientY - rect.top) * (canvas.height / rect.height);
        
        const dayIndex = getDayFromCoordinates(x, y, canvas.width, canvas.height);
        if (dayIndex >= 0 && dayIndex < 366) {
            selectDay(dayIndex);
        }
    });
    
    // ✅ TOOLTIP CORRIGÉ - Ajouter le tooltip au survol
    canvas.addEventListener('mousemove', (event) => {
        const rect = canvas.getBoundingClientRect();
        const x = (event.clientX - rect.left) * (canvas.width / rect.width);
        const y = (event.clientY - rect.top) * (canvas.height / rect.height);
        
        const dayIndex = getDayFromCoordinates(x, y, canvas.width, canvas.height);
        if (dayIndex >= 0 && dayIndex < 366 && seasonalData && seasonalData[dayIndex]) {
            showTooltip(event, dayIndex);
        } else {
            hideTooltip();
        }
    });
    
    canvas.addEventListener('mouseleave', hideTooltip);
    
    container.appendChild(canvas);
}

// ✅ TOOLTIP ENTIÈREMENT CORRIGÉ - Affichage du tooltip amélioré avec date 2025
function showTooltip(event, dayIndex) {
    let tooltip = document.getElementById('heatmapTooltip');
    if (!tooltip) {
        tooltip = document.createElement('div');
        tooltip.id = 'heatmapTooltip';
        tooltip.className = 'fixed bg-gray-800 text-white p-3 rounded-lg shadow-lg text-sm z-50 pointer-events-none border border-gray-600';
        tooltip.style.maxWidth = '250px';
        document.body.appendChild(tooltip);
    }
    
    // ✅ Calculer la date correctement pour 2025 - CORRECTION DU DÉCALAGE
    const currentYear = new Date().getFullYear(); // 2025
    const date = new Date(currentYear, 0, dayIndex + 1); // +1 car dayIndex commence à 0
    const dateStr = date.toLocaleDateString('fr-FR', { 
        weekday: 'long', 
        year: 'numeric', 
        month: 'long', 
        day: 'numeric' 
    });
    
    // ✅ Calculer la température moyenne du jour depuis seasonalData
    let avgTemp = 22;
    let minTemp = 20;
    let maxTemp = 25;
    
    if (seasonalData && seasonalData[dayIndex] && Array.isArray(seasonalData[dayIndex])) {
        const dayTemps = seasonalData[dayIndex];
        if (dayTemps.length === 24) {
            const validTemps = dayTemps.filter(temp => typeof temp === 'number' && !isNaN(temp));
            if (validTemps.length > 0) {
                avgTemp = validTemps.reduce((sum, temp) => sum + temp, 0) / validTemps.length;
                minTemp = Math.min(...validTemps);
                maxTemp = Math.max(...validTemps);
            }
        }
    }
    
    tooltip.innerHTML = `
        <div class="font-semibold text-blue-300">${dateStr}</div>
        <div class="text-gray-300">Jour ${dayIndex + 1} de l'année</div>
        <div class="mt-2">
            <div>🌡️ Moyenne: <span class="font-semibold">${avgTemp.toFixed(1)}°C</span></div>
            <div>❄️ Min: ${minTemp.toFixed(1)}°C | 🔥 Max: ${maxTemp.toFixed(1)}°C</div>
        </div>
        <div class="text-xs text-gray-400 mt-2 border-t border-gray-600 pt-2">
            🖱️ Cliquez pour éditer cette journée
        </div>
    `;
    
    // ✅ Positionner le tooltip de manière intelligente
    const tooltipRect = tooltip.getBoundingClientRect();
    let left = event.pageX + 15;
    let top = event.pageY - 10;
    
    // Ajuster si le tooltip dépasse de l'écran
    if (left + 250 > window.innerWidth) {  // Utiliser la largeur max du tooltip
        left = event.pageX - 250 - 15;
    }
    if (top + 120 > window.innerHeight) {  // Estimation de la hauteur du tooltip
        top = event.pageY - 120 - 10;
    }
    
    tooltip.style.left = `${left}px`;
    tooltip.style.top = `${top}px`;
    tooltip.style.display = 'block';
    tooltip.style.opacity = '1';
}

// Masquer le tooltip
function hideTooltip() {
    const tooltip = document.getElementById('heatmapTooltip');
    if (tooltip) {
        tooltip.style.display = 'none';
        tooltip.style.opacity = '0';
    }
}

// Dessin de la heatmap annuelle amélioré - Largeur complète
function drawYearlyHeatmap(ctx, data, width, height) {
    const { cellSize, cellPadding } = HEATMAP_CONFIG;
    const startX = 80;  // Marge pour les labels
    const startY = 30;
    const availableWidth = width - startX - 20;
    const monthWidth = availableWidth / 12;  // Largeur adaptative par mois
    const cellsPerWeek = 7;
    const weeksPerMonth = 6;  // Augmenté pour plus d'espace
    
    // Calculer les températures min/max pour la colorisation
    const allTemps = data.flat().filter(temp => typeof temp === 'number' && !isNaN(temp));
    const minTemp = Math.min(...allTemps);
    const maxTemp = Math.max(...allTemps);
    
    // Dessiner les labels des mois
    ctx.fillStyle = '#ffffff';
    ctx.font = 'bold 14px Arial';
    ctx.textAlign = 'center';
    for (let month = 0; month < 12; month++) {
        const x = startX + month * monthWidth + monthWidth / 2;
        ctx.fillText(HEATMAP_CONFIG.monthLabels[month], x, 20);
    }
    
    // Dessiner les labels des jours de la semaine
    ctx.font = '12px Arial';
    ctx.textAlign = 'right';
    for (let day = 0; day < 7; day++) {
        const y = startY + day * (cellSize + cellPadding) + cellSize / 2 + 4;
        ctx.fillText(HEATMAP_CONFIG.dayLabels[day], startX - 10, y);
    }
    
    // ✅ CORRECTION MAJEURE DU CALCUL DES POSITIONS - Dessiner les cellules pour chaque jour
    const currentYear = new Date().getFullYear(); // 2025
    data.forEach((dayTemps, dayIndex) => {
        if (!Array.isArray(dayTemps) || dayTemps.length === 0) return;
        
        const validTemps = dayTemps.filter(temp => typeof temp === 'number' && !isNaN(temp));
        if (validTemps.length === 0) return;
        
        const avgTemp = validTemps.reduce((sum, temp) => sum + temp, 0) / validTemps.length;
        
        // ✅ Calculer la position dans la grille - ENTIÈREMENT CORRIGÉ
        const date = new Date(currentYear, 0, dayIndex + 1); // +1 car dayIndex commence à 0
        const month = date.getMonth();
        const dayOfMonth = date.getDate();
        const weekday = date.getDay(); // 0 = dimanche, 1 = lundi, etc.
        
        // ✅ Calculer la semaine du mois de manière plus précise
        const firstDayOfMonth = new Date(currentYear, month, 1);
        const firstWeekday = firstDayOfMonth.getDay();
        
        // Calculer dans quelle semaine du mois se trouve ce jour
        const weekOfMonth = Math.floor((dayOfMonth + firstWeekday - 1) / 7);
        
        // ✅ Vérifier que la position est valide
        if (weekOfMonth >= 6) return; // Éviter les débordements
        
        const cellWidth = Math.min(cellSize, (monthWidth - cellPadding * 6) / 5);
        const x = startX + month * monthWidth + weekOfMonth * (cellWidth + cellPadding);
        const y = startY + weekday * (cellSize + cellPadding);
        
        // Couleur basée sur la température
        const intensity = (avgTemp - minTemp) / (maxTemp - minTemp);
        const color = getTemperatureColor(intensity);
        
        ctx.fillStyle = color;
        ctx.fillRect(x, y, cellWidth, cellSize);
        
        // ✅ Bordure pour le jour sélectionné - CORRIGÉE
        if (dayIndex === currentSelectedDay) {
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 3;
            ctx.strokeRect(x - 1, y - 1, cellWidth + 2, cellSize + 2);
        }
    });
    
    // Ajouter une légende de couleurs
    drawColorLegend(ctx, width, height, minTemp, maxTemp);
}

// Dessiner la légende des couleurs améliorée
function drawColorLegend(ctx, width, height, minTemp, maxTemp) {
    const legendWidth = 300;
    const legendHeight = 15;
    const legendX = (width - legendWidth) / 2;  // Centré
    const legendY = height - 40;
    
    // Dessiner le gradient
    const gradient = ctx.createLinearGradient(legendX, 0, legendX + legendWidth, 0);
    gradient.addColorStop(0, getTemperatureColor(0));
    gradient.addColorStop(0.2, getTemperatureColor(0.2));
    gradient.addColorStop(0.4, getTemperatureColor(0.4));
    gradient.addColorStop(0.6, getTemperatureColor(0.6));
    gradient.addColorStop(0.8, getTemperatureColor(0.8));
    gradient.addColorStop(1, getTemperatureColor(1));
    
    ctx.fillStyle = gradient;
    ctx.fillRect(legendX, legendY, legendWidth, legendHeight);
    
    // Bordure de la légende
    ctx.strokeStyle = '#666666';
    ctx.lineWidth = 1;
    ctx.strokeRect(legendX, legendY, legendWidth, legendHeight);
    
    // Labels de la légende
    ctx.fillStyle = '#ffffff';
    ctx.font = '12px Arial';
    ctx.textAlign = 'left';
    ctx.fillText(`${minTemp.toFixed(1)}°C`, legendX, legendY + legendHeight + 20);
    ctx.textAlign = 'center';
    ctx.fillText(`${((minTemp + maxTemp) / 2).toFixed(1)}°C`, legendX + legendWidth / 2, legendY + legendHeight + 20);
    ctx.textAlign = 'right';
    ctx.fillText(`${maxTemp.toFixed(1)}°C`, legendX + legendWidth, legendY + legendHeight + 20);
}

// Obtenir la couleur selon la température (amélioré)
function getTemperatureColor(intensity) {
    // Gradient plus naturel du bleu au rouge
    const colors = [
        [0, 100, 255],    // Bleu froid
        [0, 200, 200],    // Cyan
        [100, 255, 100],  // Vert
        [255, 255, 0],    // Jaune
        [255, 150, 0],    // Orange
        [255, 0, 0]       // Rouge chaud
    ];
    
    const scaledIntensity = Math.max(0, Math.min(1, intensity));
    const colorIndex = scaledIntensity * (colors.length - 1);
    const lowerIndex = Math.floor(colorIndex);
    const upperIndex = Math.ceil(colorIndex);
    const ratio = colorIndex - lowerIndex;
    
    if (lowerIndex === upperIndex) {
        const [r, g, b] = colors[lowerIndex];
        return `rgb(${r}, ${g}, ${b})`;
    }
    
    const [r1, g1, b1] = colors[lowerIndex];
    const [r2, g2, b2] = colors[upperIndex];
    
    const r = Math.round(r1 + (r2 - r1) * ratio);
    const g = Math.round(g1 + (g2 - g1) * ratio);
    const b = Math.round(b1 + (b2 - b1) * ratio);
    
    return `rgb(${r}, ${g}, ${b})`;
}

// ✅ CORRECTION MAJEURE - Obtenir le jour à partir des coordonnées de clic - ENTIÈREMENT REVU
function getDayFromCoordinates(x, y, canvasWidth, canvasHeight) {
    const { cellSize, cellPadding } = HEATMAP_CONFIG;
    const startX = 80;
    const startY = 30;
    const availableWidth = canvasWidth - startX - 20;
    const monthWidth = availableWidth / 12;
    
    // ✅ Calculer le mois
    const month = Math.floor((x - startX) / monthWidth);
    if (month < 0 || month >= 12) return -1;
    
    // ✅ Calculer la semaine dans le mois
    const cellWidth = Math.min(cellSize, (monthWidth - cellPadding * 6) / 5);
    const weekInMonth = Math.floor((x - startX - month * monthWidth) / (cellWidth + cellPadding));
    if (weekInMonth < 0 || weekInMonth >= 6) return -1;
    
    // ✅ Calculer le jour de la semaine
    const weekday = Math.floor((y - startY) / (cellSize + cellPadding));
    if (weekday < 0 || weekday >= 7) return -1;
    
    // ✅ NOUVELLE MÉTHODE : Parcourir tous les jours pour trouver celui qui correspond
    const currentYear = new Date().getFullYear(); // 2025
    
    // Parcourir tous les jours de l'année pour trouver celui qui correspond aux coordonnées
    for (let dayIndex = 0; dayIndex < 366; dayIndex++) {
        const date = new Date(currentYear, 0, dayIndex + 1);
        
        // Vérifier si cette date correspond au mois cliqué
        if (date.getMonth() !== month) continue;
        
        const dayOfMonth = date.getDate();
        const dayWeekday = date.getDay();
        
        // Calculer la position de ce jour
        const firstDayOfMonth = new Date(currentYear, month, 1);
        const firstWeekday = firstDayOfMonth.getDay();
        const dayWeekOfMonth = Math.floor((dayOfMonth + firstWeekday - 1) / 7);
        
        // Vérifier si ce jour correspond aux coordonnées cliquées
        if (dayWeekOfMonth === weekInMonth && dayWeekday === weekday) {
            return dayIndex;
        }
    }
    
    return -1; // Aucun jour trouvé
}

// Sélection d'un jour pour édition
async function selectDay(dayIndex) {
    currentSelectedDay = dayIndex;
    
    // Mettre à jour la heatmap pour montrer la sélection
    if (seasonalData) {
        createHeatmap(seasonalData);
    }
    
    // Charger les données du jour sélectionné
    await loadDayData(dayIndex);
    
    // Afficher les contrôles de navigation
    showDayNavigationControls(dayIndex);
}

// Chargement des données d'un jour spécifique
async function loadDayData(dayIndex) {
    try {
        // Utiliser les données de démonstration
        if (seasonalData && seasonalData[dayIndex]) {
            const dayData = { temperatures: seasonalData[dayIndex] };
            
            // Mettre à jour l'éditeur de courbe avec les données du jour
            if (typeof window.temperatureData !== 'undefined') {
                window.temperatureData = [...dayData.temperatures];
                
                // Mettre à jour le graphique principal si disponible
                if (typeof window.updateChartAndGrid === 'function') {
                    window.updateChartAndGrid();
                }
            }
            
            return dayData;
        }
        
        return null;
        
    } catch (error) {
        console.error(`❌ Erreur lors du chargement du jour ${dayIndex}:`, error);
        return null;
    }
}

// Affichage des contrôles de navigation pour le jour sélectionné - CORRIGÉ POUR 2025
function showDayNavigationControls(dayIndex) {
    const dayInfo = document.getElementById('selectedDayInfo');
    const navigationControls = document.getElementById('dayNavigationControls');
    
    if (!dayInfo || !navigationControls) return;
    
    // ✅ Calculer la date pour 2025 - CORRECTION DU DÉCALAGE
    const currentYear = new Date().getFullYear(); // 2025
    const date = new Date(currentYear, 0, dayIndex + 1); // +1 car dayIndex commence à 0
    const dateStr = date.toLocaleDateString('fr-FR', { 
        weekday: 'long', 
        year: 'numeric', 
        month: 'long', 
        day: 'numeric' 
    });
    
    dayInfo.innerHTML = `
        <div class="font-semibold text-white">📅 Édition du ${dateStr}</div>
        <div class="text-sm text-gray-400">
            Jour ${dayIndex + 1} de l'année • Modifiez la courbe ci-dessus en cliquant et glissant
        </div>
    `;
    
    // Afficher les contrôles de navigation
    navigationControls.style.display = 'flex';
    
    // Faire défiler vers la courbe d'édition
    const chartContainer = document.getElementById('temperatureChart');
    if (chartContainer) {
        chartContainer.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}

// Sauvegarde des modifications d'un jour
async function saveDayModifications() {
    if (currentSelectedDay === null) {
        alert('❌ Aucun jour sélectionné');
        return;
    }
    
    try {
        // Récupérer les données de température actuelles
        const temperatures = window.temperatureData || [];
        
        if (temperatures.length !== 24) {
            throw new Error('Données de température invalides');
        }
        
        // Mettre à jour les données locales
        if (seasonalData && seasonalData[currentSelectedDay]) {
            seasonalData[currentSelectedDay] = [...temperatures];
        }
        
        // Recréer la heatmap
        createHeatmap(seasonalData);
        
        // Afficher la confirmation
        showNotification('✅ Modifications sauvegardées pour ce jour', 'success');
        
    } catch (error) {
        console.error('❌ Erreur lors de la sauvegarde:', error);
        showNotification('❌ Erreur lors de la sauvegarde: ' + error.message, 'error');
    }
}

// Navigation entre les jours
function navigateDay(direction) {
    if (currentSelectedDay === null) return;
    
    const newDay = currentSelectedDay + direction;
    if (newDay >= 0 && newDay < 366) {
        selectDay(newDay);
    }
}

// Fermeture de l'éditeur de jour
function closeDayEditor() {
    currentSelectedDay = null;
    
    // Masquer les contrôles de navigation
    const navigationControls = document.getElementById('dayNavigationControls');
    const dayInfo = document.getElementById('selectedDayInfo');
    
    if (navigationControls) {
        navigationControls.style.display = 'none';
    }
    
    if (dayInfo) {
        dayInfo.innerHTML = `
            <div class="font-semibold">📅 Mode Saisonnier Activé</div>
            <div class="text-sm text-gray-400">Sélectionnez un jour dans le calendrier ci-dessous pour l'éditer</div>
        `;
    }
    
    // Recréer la heatmap sans sélection
    if (seasonalData) {
        createHeatmap(seasonalData);
    }
    
    // Nettoyer le tooltip
    hideTooltip();
}

// Outils de modification de masse
async function applyMassModification(type, params) {
    try {
        // Pour l'instant, simuler les modifications
        showNotification('✅ Modification de masse appliquée (simulation)', 'success');
        
    } catch (error) {
        console.error('❌ Erreur lors de la modification de masse:', error);
        showNotification('❌ Erreur: ' + error.message, 'error');
    }
}

// Plafonnement des températures pour un mois
function capTemperaturesForMonth() {
    const month = prompt('Mois (1-12):');
    const maxTemp = prompt('Température maximum (°C):');
    const minTemp = prompt('Température minimum (°C):');
    
    if (month && maxTemp && minTemp) {
        applyMassModification('cap_month', {
            month: parseInt(month),
            maxTemp: parseFloat(maxTemp),
            minTemp: parseFloat(minTemp)
        });
    }
}

// Lissage des courbes pour un mois
function smoothCurvesForMonth() {
    const month = prompt('Mois à lisser (1-12):');
    
    if (month) {
        applyMassModification('smooth_month', {
            month: parseInt(month)
        });
    }
}

// Copier/coller de courbes
function copyDayToMonth() {
    if (currentSelectedDay === null) {
        alert('❌ Veuillez d\'abord sélectionner un jour à copier');
        return;
    }
    
    const month = prompt('Mois de destination (1-12):');
    
    if (month) {
        applyMassModification('copy_day_to_month', {
            sourceDay: currentSelectedDay,
            targetMonth: parseInt(month)
        });
    }
}

// Initialisation des événements
function initSeasonalEventListeners() {
    // Bouton d'initialisation
    const initBtn = document.getElementById('initSeasonalBtn');
    if (initBtn) {
        initBtn.addEventListener('click', initializeSeasonalData);
    }
    
    // Boutons de navigation
    const prevBtn = document.getElementById('prevDayBtn');
    const nextBtn = document.getElementById('nextDayBtn');
    
    if (prevBtn) prevBtn.addEventListener('click', () => navigateDay(-1));
    if (nextBtn) nextBtn.addEventListener('click', () => navigateDay(1));
    
    // Bouton de sauvegarde
    const saveBtn = document.getElementById('saveDayBtn');
    if (saveBtn) {
        saveBtn.addEventListener('click', saveDayModifications);
    }
    
    // Outils de modification de masse
    const capBtn = document.getElementById('capTempsBtn');
    const smoothBtn = document.getElementById('smoothMonthBtn');
    const copyBtn = document.getElementById('copyDayBtn');
    
    if (capBtn) capBtn.addEventListener('click', capTemperaturesForMonth);
    if (smoothBtn) smoothBtn.addEventListener('click', smoothCurvesForMonth);
    if (copyBtn) copyBtn.addEventListener('click', copyDayToMonth);
}

// Création du conteneur de heatmap
function createHeatmapContainer() {
    const container = document.getElementById('heatmapCanvas');
    if (!container) {
        console.warn('⚠️ Conteneur heatmapCanvas non trouvé');
        return;
    }
    
    // Ajouter les styles nécessaires pour largeur complète
    container.style.border = '1px solid #374151';
    container.style.borderRadius = '8px';
    container.style.backgroundColor = '#1f2937';
    container.style.padding = '15px';
    container.style.width = '100%';
    container.style.boxSizing = 'border-box';
}

// Fonction utilitaire pour les notifications
function showNotification(message, type = 'info') {
    // Créer une notification temporaire
    const notification = document.createElement('div');
    notification.className = `fixed top-4 right-4 p-4 rounded-lg shadow-lg z-50 ${
        type === 'success' ? 'bg-green-600' : 
        type === 'error' ? 'bg-red-600' : 'bg-blue-600'
    } text-white`;
    notification.textContent = message;
    
    document.body.appendChild(notification);
    
    // Supprimer après 3 secondes
    setTimeout(() => {
        if (notification.parentNode) {
            notification.parentNode.removeChild(notification);
        }
    }, 3000);
}

// Export des fonctions principales
window.initSeasonalSystem = initSeasonalSystem;
window.updateSeasonalVisibility = updateSeasonalVisibility;
window.selectDay = selectDay;
window.saveDayModifications = saveDayModifications;
window.navigateDay = navigateDay;
window.closeDayEditor = closeDayEditor;
window.initializeSeasonalData = initializeSeasonalData;