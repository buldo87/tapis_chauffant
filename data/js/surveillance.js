// js/surveillance.js
  
// Chart.js configuration with moving averages
const chartOptions = {
	responsive: true,
	maintainAspectRatio: false,
	plugins: {
		legend: {
			labels: {
				color: '#ffffff'
			}
		}
	},
	scales: {
		x: {
			ticks: { color: '#cccccc' },
			grid: { color: '#404040' }
		},
		y: {
			ticks: { color: '#cccccc' },
			grid: { color: '#404040' }
		}
	}
};

// Calculate moving average
function calculateMovingAverage(data, windowSize) {
	const result = [];
	for (let i = 0; i < data.length; i++) {
		const start = Math.max(0, i - windowSize + 1);
		const window = data.slice(start, i + 1);
		const average = window.reduce((sum, val) => sum + val, 0) / window.length;
		result.push(average);
	}
	return result;
}

// Initialize charts
function initCharts() {
	const tempCtx = document.getElementById('tempChart').getContext('2d');
	tempChart = new Chart(tempCtx, {
		type: 'line',
		data: {
			labels: [],
			datasets: [
				{
					label: 'Température',
					data: [],
					borderColor: '#ff6b35',
					backgroundColor: 'rgba(255, 107, 53, 0.1)',
					borderWidth: 2,
					fill: true,
					tension: 0.3,
					pointRadius: 1,
					pointHoverRadius: 4
				},
				{
					label: 'Moyenne 24h',
					data: [],
					borderColor: '#ffab00',
					backgroundColor: 'transparent',
					borderWidth: 3,
					borderDash: [5, 5],
					fill: false,
					tension: 0.3,
					pointRadius: 1,
				}
			]
		},
		options: chartOptions
	});

	const humCtx = document.getElementById('humidityChart').getContext('2d');
	humidityChart = new Chart(humCtx, {
		type: 'line',
		data: {
			labels: [],
			datasets: [
				{
					label: 'Humidité',
					data: [],
					borderColor: '#3b82f6',
					backgroundColor: 'rgba(59, 130, 246, 0.1)',
					borderWidth: 2,
					fill: true,
					tension: 0.3,
					pointRadius: 1,

				},
				{
					label: 'Moyenne 24h',
					data: [],
					borderColor: '#60a5fa',
					backgroundColor: 'transparent',
					borderWidth: 3,
					borderDash: [5, 5],
					fill: false,
					tension: 0.3,
					pointRadius: 1,

				}
			]
		},
		options: chartOptions
	});
}
