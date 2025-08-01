export const state = {
    // Données des capteurs et de l'état
    status: {
        temperature: 0,
        humidity: 0,
        heaterState: 0,
        currentMode: '--',
        setpoint: 0,
        sensorValid: true,
        safetyLevel: 0,
        emergencyShutdown: false,
        lastUpdate: 0
    },

    // Configuration chargée depuis l'ESP
    config: {
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
    },

    // État de l'interface utilisateur
    ui: {
        isDragging: false,
        currentTab: 'dashboard', // 'dashboard' ou 'config'
        cameraVisible: false,
        seasonalView: 'heatmap' // 'heatmap', 'editor'
    },

    // Données pour les graphiques
    charts: {
        tempHistory: [],
        humidityHistory: [],
        timeLabels: []
    },

    // Données saisonnières
    seasonalData: null // Sera un tableau de 366x24
};

// Fonctions pour mettre à jour l'état de manière contrôlée
export function updateStatus(newStatus) {
    // Update current status properties
    Object.assign(state.status, newStatus);

    // Update history for charts and stats
    const MAX_HISTORY_LENGTH = 100; // Same as chart limit

    if (newStatus.temperature !== undefined) {
        state.charts.tempHistory.push(newStatus.temperature / 10.0);
        if (state.charts.tempHistory.length > MAX_HISTORY_LENGTH) {
            state.charts.tempHistory.shift();
        }
    }

    if (newStatus.humidity !== undefined) {
        state.charts.humidityHistory.push(newStatus.humidity / 10.0);
        if (state.charts.humidityHistory.length > MAX_HISTORY_LENGTH) {
            state.charts.humidityHistory.shift();
        }
    }
}

export function setConfig(newConfig) {
    Object.assign(state.config, newConfig);
}

export function setUiState(newUiState) {
    Object.assign(state.ui, newUiState);
}
