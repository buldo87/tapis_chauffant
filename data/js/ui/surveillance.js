/**
 * @file ui/surveillance.js
 * @description Gère la mise à jour des éléments de l'onglet "Surveillance".
 */

import { state } from '../state.js';

// Références aux éléments du DOM
const elements = {
    temperature: document.getElementById('temperature'),
    minTemperature: document.getElementById('minTemperature'),
    maxTemperature: document.getElementById('maxTemperature'),
    movingAverageTemp: document.getElementById('movingAverageTemp'),
    humidity: document.getElementById('humidity'),
    minHumidite: document.getElementById('minHumidite'),
    maxHumidite: document.getElementById('maxHumidite'),
    movingAverageHum: document.getElementById('movingAverageHum'),
    heaterState: document.getElementById('heaterState'),
    currentMode: document.getElementById('currentMode'),
    modeDetails: document.getElementById('modeDetails'),
    consigneTemp: document.getElementById('consigneTemp'),
    ledDot: document.getElementById('led-dot'),
    currentTime: document.getElementById('currentTime'),
    tempChartCanvas: document.getElementById('tempChart'),
    humidityChartCanvas: document.getElementById('humidityChart')
};

let tempChart, humidityChart;

function createChart(canvas, label, color) {
    if (!canvas) return null;
    const ctx = canvas.getContext('2d');
    return new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: label,
                data: [],
                borderColor: color,
                backgroundColor: `${color}1a`,
                borderWidth: 2,
                fill: true,
                tension: 0.3
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    ticks: { display: false }
                }
            }
        }
    });
}

export function initSurveillance() {
    tempChart = createChart(elements.tempChartCanvas, 'Température', '#ff6b6b');
    humidityChart = createChart(elements.humidityChartCanvas, 'Humidité', '#4dabf7');
}

function calculateStats(data) {
    if (data.length === 0) {
        return { min: '--', max: '--', avg: '--' };
    }
    const min = Math.min(...data);
    const max = Math.max(...data);
    const sum = data.reduce((a, b) => a + b, 0);
    const avg = sum / data.length;
    return { min: min.toFixed(1), max: max.toFixed(1), avg: avg.toFixed(1) };
}

export function updateSurveillance(status) {
    // Mise à jour des cartes
    elements.temperature.textContent = `${status.temperature !== undefined ? (status.temperature / 10.0).toFixed(1) : '--'}°C`;

    const tempStats = calculateStats(state.charts.tempHistory);
    elements.minTemperature.textContent = `${tempStats.min}°C`;
    elements.maxTemperature.textContent = `${tempStats.max}°C`;
    elements.movingAverageTemp.textContent = `${tempStats.avg}°C`;

    elements.humidity.textContent = `${status.humidity !== undefined ? status.humidity.toFixed(1) : '--'}%`;

    const humStats = calculateStats(state.charts.humidityHistory);
    elements.minHumidite.textContent = `${humStats.min}%`;
    elements.maxHumidite.textContent = `${humStats.max}%`;
    elements.movingAverageHum.textContent = `${humStats.avg}%`;

    elements.heaterState.textContent = status.heaterState !== undefined ? status.heaterState : '--';
    elements.currentMode.textContent = status.currentMode !== undefined ? status.currentMode : '--';
    elements.consigneTemp.textContent = `${status.consigneTemp !== undefined ? (status.consigneTemp / 10.0).toFixed(1) : '--'}°C`;

    // Mode Details (PID ou Hystérésis)
    if (status.currentMode === 'PID' && status.Kp !== undefined && status.Ki !== undefined && status.Kd !== undefined) {
        elements.modeDetails.textContent = `Kp: ${(status.Kp / 10.0).toFixed(1)}, Ki: ${(status.Ki / 10.0).toFixed(1)}, Kd: ${(status.Kd / 10.0).toFixed(1)}`;
    } else if (status.currentMode === 'Hysteresis' && status.hysteresis !== undefined) {
        elements.modeDetails.textContent = `Hystérésis: ${(status.hysteresis / 10.0).toFixed(1)}°C`;
    } else {
        elements.modeDetails.textContent = '';
    }

    // LED State
    if (status.ledState !== undefined) {
        if (status.ledState && status.ledRed !== undefined && status.ledGreen !== undefined && status.ledBlue !== undefined) {
            elements.ledDot.style.backgroundColor = `rgb(${status.ledRed}, ${status.ledGreen}, ${status.ledBlue})`;
        } else {
            elements.ledDot.style.backgroundColor = '#000'; // Off
        }
    } else {
        elements.ledDot.style.backgroundColor = '#000'; // Default to off if state is undefined
    }

    // Current Time
    elements.currentTime.textContent = status.currentTime !== undefined ? new Date(status.currentTime * 1000).toLocaleTimeString() : '--:--';

    // Mise à jour des graphiques
    const timeLabel = new Date().toLocaleTimeString();
    updateChartData(tempChart, timeLabel, status.temperature / 10.0);
    updateChartData(humidityChart, timeLabel, status.humidity / 10.0);
}

function updateChartData(chart, label, value) {
    if (!chart) return;
    chart.data.labels.push(label);
    chart.data.datasets[0].data.push(value);

    // Limiter le nombre de points sur le graphique
    if (chart.data.labels.length > 100) {
        chart.data.labels.shift();
        chart.data.datasets[0].data.shift();
    }
    chart.update('none');
}

// Initialisation au chargement du module
initSurveillance();