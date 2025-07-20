/**
 * @file ui/surveillance.js
 * @description Gère la mise à jour des éléments de l'onglet "Surveillance".
 */

import { state } from '../state.js';

// Références aux éléments du DOM
const elements = {
    temperature: document.getElementById('temperature'),
    humidity: document.getElementById('humidity'),
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

export function updateSurveillance(status) {
    // Mise à jour des cartes
    elements.temperature.textContent = `${status.temperature.toFixed(1)}°C`;
    elements.humidity.textContent = `${status.humidity.toFixed(1)}%`;

    // Mise à jour des graphiques
    const timeLabel = new Date().toLocaleTimeString();
    updateChartData(tempChart, timeLabel, status.temperature);
    updateChartData(humidityChart, timeLabel, status.humidity);
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