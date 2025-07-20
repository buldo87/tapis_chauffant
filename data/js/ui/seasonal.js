import { api } from '../api.js';
import { state } from '../state.js';
import { updateTempChart, getTempCurve } from './configuration.js';

const heatmapContainer = document.getElementById('seasonalHeatmap');
const heatmapCanvas = document.getElementById('heatmapCanvas');
const heatmapLoadingSpinner = document.getElementById('heatmapLoadingSpinner');
const selectedDayInfo = document.getElementById('selectedDayInfo'); 
const dayNavigationControls = document.getElementById('dayNavigationControls'); 
const saveDayBtn = document.getElementById('saveDayBtn');
const applyYearlyBtn = document.getElementById('applyYearlyBtn');

let selectedDayIndex = 0; // Toujours commencer par le premier jour

function getYearlyTemperatureRange() {
    let min = Infinity;
    let max = -Infinity;
    if (!state.seasonalData || state.seasonalData.length === 0) {
        return { min: 10, max: 35 };
    }
    // Iterate over the already divided by 10 values in state.seasonalData
    for (let day = 0; day < state.seasonalData.length; day++) {
        for (let h = 0; h < 24; h++) {
            const t = state.seasonalData[day][h];
            if (typeof t === 'number' && !isNaN(t)) {
                if (t < min) min = t;
                if (t > max) max = t;
            }
        }
    }
    return { min, max };
}

function tempToColor(temp) {
    const { min, max } = getYearlyTemperatureRange();
    if (max === min) return `hsl(120, 100%, 50%)`;
    const ratio = Math.max(0, Math.min(1, (temp - min) / (max - min)));
    const hue = (1 - ratio) * 240;
    return `hsl(${hue}, 100%, 50%)`;
}

function getMonthName(monthIndex) {
    const months = ['Jan', 'Fév', 'Mar', 'Avr', 'Mai', 'Juin', 'Juil', 'Aoû', 'Sep', 'Oct', 'Nov', 'Déc'];
    return months[monthIndex];
}

function getDayInfo(dayIndex) {
    const date = new Date(new Date().getFullYear(), 0, dayIndex + 1);
    const month = date.getMonth();
    const dayOfMonth = date.getDate();
    const dayData = state.seasonalData[dayIndex];
    if (!dayData || dayData.length === 0) {
        return { date: `${dayOfMonth} ${getMonthName(month)}`, avgTemp: 'N/A', minTemp: 'N/A', maxTemp: 'N/A', tooltip: `Date: ${dayOfMonth} ${getMonthName(month)} - Données manquantes` };
    }
    const avgTemp = dayData.reduce((a, b) => a + b, 0) / dayData.length;
    const minTemp = Math.min(...dayData);
    const maxTemp = Math.max(...dayData);
    return { date: `${dayOfMonth} ${getMonthName(month)}`, avgTemp: avgTemp.toFixed(1), minTemp: minTemp.toFixed(1), maxTemp: maxTemp.toFixed(1), tooltip: `Date: ${dayOfMonth} ${getMonthName(month)}
Moy: ${avgTemp.toFixed(1)}°C\nMin: ${minTemp.toFixed(1)}°C\nMax: ${maxTemp.toFixed(1)}°C` };
}

function calculateYearlyStats(yearlyData) {
    let allTemps = [];
    yearlyData.forEach(dayData => {
        allTemps = allTemps.concat(dayData);
    });

    if (allTemps.length === 0) {
        return { min: '--', max: '--', avg: '--' };
    }

    const min = Math.min(...allTemps);
    const max = Math.max(...allTemps);
    const sum = allTemps.reduce((a, b) => a + b, 0);
    const avg = sum / allTemps.length;

    return { min: min.toFixed(1), max: max.toFixed(1), avg: avg.toFixed(1) };
}

function renderHeatmap(yearlyData) {
    if (!heatmapCanvas) return;
    if (!yearlyData || yearlyData.length === 0) {
        heatmapCanvas.innerHTML = '<p class="text-gray-500">Aucune donnée annuelle disponible.</p>';
        document.getElementById('minYearlyTemp').textContent = '--°C';
        document.getElementById('maxYearlyTemp').textContent = '--°C';
        document.getElementById('avgYearlyTemp').textContent = '--°C';
        return;
    }

    const stats = calculateYearlyStats(yearlyData);
    document.getElementById('minYearlyTemp').textContent = `${stats.min}°C`;
    document.getElementById('maxYearlyTemp').textContent = `${stats.max}°C`;
    document.getElementById('avgYearlyTemp').textContent = `${stats.avg}°C`;

    let html = '';
    const daysInMonth = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    const dayOfWeekLabels = ['L', 'M', 'M', 'J', 'V', 'S', 'D'];
    html += `<div class="yearly-calendar-wrapper">`;
    html += `<div class="day-of-week-column">`;
    html += `<div class="empty-cell-for-month-header"></div>`;
    dayOfWeekLabels.forEach(label => { html += `<div class="day-of-week-header">${label}</div>`; });
    html += `</div>`;
    html += `<div class="months-container">`;
    let dayIndex = 0;
    let currentYear = new Date().getFullYear();
    for (let month = 0; month < 12; month++) {
        html += `<div class="month-column">`;
        html += `<div class="month-grid">`;
        const firstDayOfMonth = new Date(currentYear, month, 1).getDay();
        const startOffset = (firstDayOfMonth === 0) ? 6 : firstDayOfMonth - 1;
        for (let i = 0; i < startOffset; i++) { html += `<div class="heatmap-cell empty-cell"></div>`; }
        for (let day = 0; day < daysInMonth[month]; day++) {
            if (dayIndex < yearlyData.length) {
                const dayInfo = getDayInfo(dayIndex);
                const avgTemp = dayInfo.avgTemp !== 'N/A' ? parseFloat(dayInfo.avgTemp) : null;
                const color = avgTemp !== null ? tempToColor(avgTemp) : '#333';
                const currentDayOfWeek = (startOffset + day) % 7;
                const currentWeekOfMonth = Math.floor((startOffset + day) / 7);
                html += `<div class="heatmap-cell ${selectedDayIndex === dayIndex ? 'selected-day' : ''}" style="background-color: ${color}; grid-row: ${currentDayOfWeek + 1}; grid-column: ${currentWeekOfMonth + 1};" data-day-index="${dayIndex}" title="${dayInfo.tooltip}"></div>`;
            } else {
                html += `<div class="heatmap-cell empty-cell"></div>`;
            }
            dayIndex++;
        }
        html += `</div></div>`;
    }
    html += `</div></div>`;
    heatmapCanvas.innerHTML = html;
    heatmapCanvas.querySelectorAll('.heatmap-cell').forEach(cell => {
        cell.addEventListener('click', (e) => {
            const index = parseInt(e.target.dataset.dayIndex);
            if (!isNaN(index) && state.seasonalData[index]) {
                selectDay(index);
            }
        });
    });
}

function selectDay(index) {
    if (selectedDayIndex !== -1) {
        const prevSelectedCell = heatmapCanvas.querySelector(`[data-day-index="${selectedDayIndex}"]`);
        if (prevSelectedCell) prevSelectedCell.classList.remove('selected-day');
    }
    selectedDayIndex = index;
    const newSelectedCell = heatmapCanvas.querySelector(`[data-day-index="${selectedDayIndex}"]`);
    if (newSelectedCell) newSelectedCell.classList.add('selected-day');
    if (state.seasonalData[selectedDayIndex]) {
        updateTempChart(state.seasonalData[selectedDayIndex]);
        const dayInfo = getDayInfo(selectedDayIndex);
        if (selectedDayInfo) {
            selectedDayInfo.innerHTML = `<div class="font-semibold">📅 Jour sélectionné: ${dayInfo.date}</div><div class="text-sm text-gray-400">Moy: ${dayInfo.avgTemp}°C | Min: ${dayInfo.minTemp}°C | Max: ${dayInfo.maxTemp}°C</div>`;
        }
        if (dayNavigationControls) dayNavigationControls.style.display = 'flex';
    }
}

async function saveSelectedDay() {
    if (selectedDayIndex === -1) return;
    const tempCurve = getTempCurve(); // This returns values * 10
    try {
        await api.saveDayData(selectedDayIndex, tempCurve);
        alert(`Données du jour ${selectedDayIndex + 1} sauvegardées.`);
        state.seasonalData[selectedDayIndex] = tempCurve;
        renderHeatmap(state.seasonalData);
    } catch (error) {
        console.error("Failed to save day data:", error);
        alert("Erreur lors de la sauvegarde des données du jour.");
    }
}

async function applyCurveToYear() {
    if (!confirm("Êtes-vous sûr de vouloir appliquer la courbe actuelle à TOUS les jours de l'année ? Cette action est irréversible.")) return;
    const tempCurve = getTempCurve(); // This returns values * 10
    try {
        await api.applyYearlyCurve(tempCurve);
        alert("La courbe a été appliquée à toute l'année.");
        const yearlyDataBuffer = await api.getYearlyTemperatures();
        const yearlyData = (await parseBinData(yearlyDataBuffer)).map(day => day.map(t => t / 10.0)); // Divide by 10 here
        state.seasonalData = yearlyData;
        renderHeatmap(yearlyData);
    } catch (error) {
        console.error("Failed to apply yearly curve:", error);
        alert("Erreur lors de l'application de la courbe à l'année.");
    }
}

import { parseBinData } from '../utils.js';

const smoothDayBtn = document.getElementById('smoothDayBtn');

function smoothCurrentDayCurve() {
    const currentCurve = getTempCurve(); // This returns values * 10
    if (!currentCurve || currentCurve.length === 0) return;

    // Convert to float for calculation
    const currentCurveFloat = currentCurve.map(t => t / 10.0);
    const smoothedCurveFloat = [...currentCurveFloat];

    for (let i = 0; i < smoothedCurveFloat.length; i++) {
        const prev = smoothedCurveFloat[(i - 1 + smoothedCurveFloat.length) % smoothedCurveFloat.length];
        const next = smoothedCurveFloat[(i + 1) % smoothedCurveFloat.length];
        smoothedCurveFloat[i] = (prev + currentCurveFloat[i] + next) / 3;
    }

    // Convert back to int16_t * 10 for updateTempChart
    const smoothedCurveInt = smoothedCurveFloat.map(t => Math.round(t * 10));
    updateTempChart(smoothedCurveInt);
}

export async function initSeasonal() {
    heatmapContainer.style.display = 'block';
    document.getElementById('seasonalTools').style.display = 'block';
    document.getElementById('seasonalNavigation').style.display = 'block';

    if (heatmapLoadingSpinner) heatmapLoadingSpinner.style.display = 'flex';
    try {
        const arrayBuffer = await api.getYearlyTemperatures();
        const yearlyData = (await parseBinData(arrayBuffer)).map(day => day.map(t => t / 10.0));
        state.seasonalData = yearlyData;
        renderHeatmap(yearlyData);
        selectDay(0); // Sélectionner le premier jour par défaut
    } catch (error) {
        console.error("Failed to load yearly data for heatmap:", error);
        heatmapCanvas.innerHTML = '<p class="text-red-500">Erreur de chargement des données annuelles.</p>';
    } finally {
        if (heatmapLoadingSpinner) heatmapLoadingSpinner.style.display = 'none';
    }

    saveDayBtn.addEventListener('click', saveSelectedDay);
    applyYearlyBtn.addEventListener('click', applyCurveToYear);
    smoothDayBtn.addEventListener('click', smoothCurrentDayCurve);

    document.getElementById('prevDayBtn').addEventListener('click', () => {
        if (selectedDayIndex > 0) selectDay(selectedDayIndex - 1);
    });
    document.getElementById('nextDayBtn').addEventListener('click', () => {
        if (selectedDayIndex < 364) selectDay(selectedDayIndex + 1);
    });
}

export function updateHeatmap(yearlyData) {
    state.seasonalData = yearlyData;
    renderHeatmap(yearlyData);
    selectDay(selectedDayIndex);
}