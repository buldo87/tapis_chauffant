import { state, setConfig } from '../state.js';
import { toggleMap } from './map.js';
import { api } from '../api.js';

let chart;
let isDragging = false;

function gatherConfigFromUI() {
    const newConfig = { ...state.config };

    newConfig.usePWM = document.getElementById('usePWM').checked;
    newConfig.hysteresis = parseFloat(document.getElementById('hysteresisSet').value);
    newConfig.Kp = parseFloat(document.getElementById('KpSet').value);
    newConfig.Ki = parseFloat(document.getElementById('KiSet').value);
    newConfig.Kd = parseFloat(document.getElementById('KdSet').value);

    newConfig.useLimitTemp = document.getElementById('useLimitTemp').checked;
    newConfig.globalMinTempSet = parseFloat(document.getElementById('minTempSet').value) * 10;
    newConfig.globalMaxTempSet = parseFloat(document.getElementById('maxTempSet').value) * 10;

    newConfig.cameraEnabled = document.getElementById('showCamera').checked;
    newConfig.cameraResolution = document.getElementById('cameraResolution').value;

    newConfig.ledState = document.getElementById('led-toggle').checked;
    newConfig.ledBrightness = parseInt(document.getElementById('brightness-slider').value);
    try {
        const colorString = $('#color-picker').spectrum("get").toHexString();
        newConfig.ledRed = parseInt(colorString.substring(1, 3), 16);
        newConfig.ledGreen = parseInt(colorString.substring(3, 5), 16);
        newConfig.ledBlue = parseInt(colorString.substring(5, 7), 16);
    } catch (e) {
        console.warn("Could not get color from spectrum picker.", e);
    }

    newConfig.weatherMode = document.getElementById('weatherMode').checked;

    return newConfig;
}

function initChart() {
    const canvas = document.getElementById('configTempChart');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    const extendedTemperatureData = [
        state.config.tempCurve[state.config.tempCurve.length - 1],
        ...state.config.tempCurve,
        state.config.tempCurve[0]
    ];

    const labels = ['23h', '0h', '1h', '2h', '3h', '4h', '5h', '6h', '7h', '8h', '9h', '10h', '11h',
                    '12h', '13h', '14h', '15h', '16h', '17h', '18h', '19h', '20h', '21h', '22h', '23h', '0h'];

    chart = new Chart(ctx, {
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
            interaction: { intersect: false, mode: 'index' },
            plugins: {
                legend: { labels: { color: '#ffffff', font: { size: 14 } } },
                title: { display: true, text: "Courbe d'édition du jour - Cliquez et glissez pour ajuster", color: '#ffffff', font: { size: 18 } }
            },
            scales: { 
                y: { 
                    min: state.config.globalMinTempSet / 10.0,
                    max: state.config.globalMaxTempSet / 10.0,
                    ticks: {
                        callback: function(value, index, values) {
                            return value.toFixed(1) + '°C';
                        }
                    }
                }
            }
        }
    });

    const dragHandler = (event) => {
        if (!isDragging) return;
        event.preventDefault();
        const rect = chart.canvas.getBoundingClientRect();
        let clientX = event.clientX, clientY = event.clientY;
        if (event.touches) { clientX = event.touches[0].clientX; clientY = event.touches[0].clientY; }
        const x = clientX - rect.left, y = clientY - rect.top;
        const xAxis = chart.scales.x, yAxis = chart.scales.y;
        if (x < xAxis.left || x > xAxis.right || y < yAxis.top || y > yAxis.bottom) { isDragging = false; return; }
        const index = xAxis.getValueForPixel(x);
        const temp = yAxis.getValueForPixel(y);
        updateTemperature(Math.round(index), temp);
    };

    const startDrag = (event) => { isDragging = true; dragHandler(event); };
    const endDrag = () => { isDragging = false; };

    chart.canvas.addEventListener('mousedown', startDrag);
    chart.canvas.addEventListener('mousemove', dragHandler);
    chart.canvas.addEventListener('mouseup', endDrag);
    chart.canvas.addEventListener('mouseleave', endDrag);
    chart.canvas.addEventListener('touchstart', startDrag, { passive: false });
    chart.canvas.addEventListener('touchmove', dragHandler, { passive: false });
    chart.canvas.addEventListener('touchend', endDrag);
}

function updateTemperature(hour, temp) {
    if (hour >= 1 && hour <= 24) {
        // Convert temp (float) to int16_t * 10 for storage
        const newTempInt = Math.round(Math.max(state.config.globalMinTempSet / 10.0, Math.min(state.config.globalMaxTempSet / 10.0, temp)) * 10);
        state.config.tempCurve[hour - 1] = newTempInt;
        
        // Convert back to float for chart display
        const newTempFloat = newTempInt / 10.0;
        chart.data.datasets[0].data[hour] = newTempFloat;
        if (hour === 1) chart.data.datasets[0].data[25] = newTempFloat;
        if (hour === 24) chart.data.datasets[0].data[0] = newTempFloat;
        chart.update('none');
    }
}

function updateChartScales() {
    if (!chart) return;
    const minVal = parseFloat(document.getElementById('minTempSet').value);
    const maxVal = parseFloat(document.getElementById('maxTempSet').value);

    state.config.globalMinTempSet = minVal * 10; // Update state with value * 10
    state.config.globalMaxTempSet = maxVal * 10; // Update state with value * 10

    chart.options.scales.y.min = minVal;
    chart.options.scales.y.max = maxVal;
    chart.update();
}

function updateVisibility() {
    document.getElementById("pwmSettings").style.display = document.getElementById("usePWM").checked ? "block" : "none";
    document.getElementById("hysteresisSettings").style.display = document.getElementById("usePWM").checked ? "none" : "block";
    document.getElementById("limitTempSettings").style.display = document.getElementById("useLimitTemp").checked ? "block" : "none";

    const weatherModeChecked = document.getElementById("weatherMode").checked;
    document.getElementById("weatherSettings").style.display = weatherModeChecked ? "block" : "none";

    updateChartScales(); // Update chart scales when visibility changes
}

export function getTempCurve() {
    return state.config.tempCurve;
}

export function initConfiguration(config, onSave) {
    setConfig(config);
    initChart();

    document.getElementById("usePWM").checked = config.usePWM;
    document.getElementById("useLimitTemp").checked = config.useLimitTemp;
    document.getElementById("showCamera").checked = config.cameraEnabled;
    document.getElementById("weatherMode").checked = config.weatherMode;

    // Initialize min/max temperature input fields
    document.getElementById('minTempSet').value = (config.globalMinTempSet / 10.0).toFixed(1);
    document.getElementById('maxTempSet').value = (config.globalMaxTempSet / 10.0).toFixed(1);

    updateVisibility();

    document.getElementById("usePWM").addEventListener('change', updateVisibility);
    document.getElementById("useLimitTemp").addEventListener('change', updateVisibility);
    document.getElementById("weatherMode").addEventListener('change', updateVisibility);

    document.getElementById("minTempSet").addEventListener('change', updateChartScales);
    document.getElementById("maxTempSet").addEventListener('change', updateChartScales);

    document.getElementById('applyBtn').addEventListener('click', async () => {
        const newConfig = gatherConfigFromUI();
        try {
            await onSave(newConfig);
            alert('Configuration sauvegardée avec succès !');
        } catch (error) {
            console.error("Erreur lors de la sauvegarde:", error);
            alert(`Erreur lors de la sauvegarde de la configuration: ${error.message || error}`);
        }
    });
}

export function updateTempChart(tempData) {
    if (!chart) return;
    state.config.tempCurve = tempData;
    const extendedTemperatureData = [
        tempData[tempData.length - 1] ,
        ...tempData.map(t => t ),
        tempData[0] 
    ];
    chart.data.datasets[0].data = extendedTemperatureData;
    chart.update();
}
