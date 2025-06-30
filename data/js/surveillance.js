// surveillance.js - Graphiques de surveillance en temps réel

// Fonction pour calculer la moyenne mobile
function calculateMovingAverage(data, windowSize) {
    if (!data || data.length === 0) return [];
    
    const result = [];
    for (let i = 0; i < data.length; i++) {
        const start = Math.max(0, i - windowSize + 1);
        const window = data.slice(start, i + 1);
        const average = window.reduce((sum, val) => sum + val, 0) / window.length;
        result.push(average);
    }
    return result;
}

// Configuration des graphiques
const chartConfig = {
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
                font: { size: 12 }
            }
        },
        tooltip: {
            backgroundColor: 'rgba(0, 0, 0, 0.8)',
            titleColor: '#ffffff',
            bodyColor: '#ffffff',
            borderColor: '#374151',
            borderWidth: 1
        }
    },
    scales: {
        x: {
            grid: { color: 'rgba(255, 255, 255, 0.1)' },
            ticks: { 
                color: '#ffffff',
                maxTicksLimit: 10
            }
        },
        y: {
            grid: { color: 'rgba(255, 255, 255, 0.1)' },
            ticks: { color: '#ffffff' }
        }
    },
    elements: {
        line: {
            tension: 0.3
        },
        point: {
            radius: 2,
            hoverRadius: 6
        }
    }
};

// Initialisation des graphiques de surveillance
function initCharts() {
    console.log('📊 Initialisation des graphiques de surveillance...');
    
    // Graphique de température
    const tempCtx = document.getElementById('tempChart');
    if (tempCtx) {
        tempChart = new Chart(tempCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Température (°C)',
                        data: [],
                        borderColor: '#f59e0b',
                        backgroundColor: 'rgba(245, 158, 11, 0.1)',
                        borderWidth: 2,
                        fill: true
                    },
                    {
                        label: 'Moyenne mobile 24h',
                        data: [],
                        borderColor: '#ef4444',
                        backgroundColor: 'transparent',
                        borderWidth: 1,
                        borderDash: [5, 5],
                        fill: false,
                        pointRadius: 0
                    }
                ]
            },
            options: {
                ...chartConfig,
                scales: {
                    ...chartConfig.scales,
                    y: {
                        ...chartConfig.scales.y,
                        title: {
                            display: true,
                            text: 'Température (°C)',
                            color: '#ffffff'
                        }
                    }
                }
            }
        });
    }
    
    // Graphique d'humidité
    const humCtx = document.getElementById('humidityChart');
    if (humCtx) {
        humidityChart = new Chart(humCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Humidité (%)',
                        data: [],
                        borderColor: '#3b82f6',
                        backgroundColor: 'rgba(59, 130, 246, 0.1)',
                        borderWidth: 2,
                        fill: true
                    },
                    {
                        label: 'Moyenne mobile 24h',
                        data: [],
                        borderColor: '#06b6d4',
                        backgroundColor: 'transparent',
                        borderWidth: 1,
                        borderDash: [5, 5],
                        fill: false,
                        pointRadius: 0
                    }
                ]
            },
            options: {
                ...chartConfig,
                scales: {
                    ...chartConfig.scales,
                    y: {
                        ...chartConfig.scales.y,
                        title: {
                            display: true,
                            text: 'Humidité (%)',
                            color: '#ffffff'
                        },
                        min: 0,
                        max: 100
                    }
                }
            }
        });
    }
    
    console.log('✅ Graphiques de surveillance initialisés');
}

// Export des fonctions pour utilisation globale
window.initCharts = initCharts;
window.calculateMovingAverage = calculateMovingAverage;