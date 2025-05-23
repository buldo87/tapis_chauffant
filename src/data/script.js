// Variables globales
let temperatureChart, humidityChart;
let isConnected = false;
let updateInterval;
let flashActive = false;

// Configuration des graphiques
const chartConfig = {
    temperature: {
        datasets: [{
            label: 'Température actuelle',
            data: [],
            borderColor: 'rgb(239, 68, 68)',
            backgroundColor: 'rgba(239, 68, 68, 0.1)',
            tension: 0.4,
            fill: true
        }, {
            label: 'Moyenne 24h',
            data: [],
            borderColor: 'rgb(59, 130, 246)',
            backgroundColor: 'rgba(59, 130, 246, 0.1)',
            tension: 0.4,
            fill: false,
            borderDash: [5, 5]
        }]
    },
    humidity: {
        datasets: [{
            label: 'Humidité actuelle',
            data: [],
            borderColor: 'rgb(16, 185, 129)',
            backgroundColor: 'rgba(16, 185, 129, 0.1)',
            tension: 0.4,
            fill: true
        }, {
            label: 'Moyenne 24h',
            data: [],
            borderColor: 'rgb(59, 130, 246)',
            backgroundColor: 'rgba(59, 130, 246, 0.1)',
            tension: 0.4,
            fill: false,
            borderDash: [5, 5]
        }]
    }
};

// Initialisation au chargement de la page
document.addEventListener('DOMContentLoaded', function() {
    initializeTheme();
    initializeCharts();
    initializeControls();
    loadConfiguration();
    startDataUpdate();
});