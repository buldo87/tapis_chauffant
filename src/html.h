const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <title>Contrôle de Température Tapis Chauffant</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>

<script>
let cameraEnabled = false; // ✅ déclaration globale
let refreshInterval = null;
function updateCameraVisibility() {
  const show = document.getElementById("showCamera").checked;
  const container = document.getElementById("cameraContainer");
  const img = document.getElementById("camStream");

  container.style.display = show ? "block" : "none";

  fetch("/setCamera?enabled=" + (show ? 1 : 0))
    .catch(err => console.error("Erreur setCamera:", err));

  if (show) {
    refreshInterval = setInterval(() => {
      img.src = "/capture?ts=" + new Date().getTime();
    }, 1000); // 1 FPS
  } else {
    clearInterval(refreshInterval);
  }
}
function updateVisibility() {
    console.log("→ updateVisibility appelée");
    const pwmChecked = document.getElementById("usePWM").checked;
    console.log("→ PWM cochée :", pwmChecked);
    const pwmDiv = document.getElementById("pwmSettings");
    if (pwmDiv) pwmDiv.style.display = pwmChecked ? "flex" : "none";
    const weatherChecked = document.getElementById("weatherMode").checked;
    console.log("→ Météo cochée :", weatherChecked);
    const weatherDiv = document.getElementById("weatherSettings");
    const weatherDisplay = document.getElementById("weatherDisplay");
    if (weatherDiv) weatherDiv.style.display = weatherChecked ? "flex" : "none";
    if (weatherDisplay) weatherDisplay.style.display = weatherChecked ? "flex" : "none";
    const cameraContainer = document.getElementById("cameraContainer");
    if (cameraContainer) cameraContainer.style.display = cameraEnabled ? "flex" : "none";
    if (weatherChecked) updateExternalWeather();
}

// Sécurité pour l'appel de la fonction
// DOM prêt
document.addEventListener("DOMContentLoaded", () => {
  document.getElementById("showCamera").addEventListener("change", updateCameraVisibility);

  fetch("/getSettings")
    .then(res => res.json())
    .then(settings => {
      const enabled = settings.cameraEnabled || false;
      document.getElementById("showCamera").checked = enabled;
      updateCameraVisibility();
    document.getElementById("usePWM").addEventListener("change", updateVisibility);
    document.getElementById("weatherMode").addEventListener("change", updateVisibility);

    })
    .catch(err => console.error("Erreur récupération paramètres:", err));
});

</script>


  <style>
    body { font-family: Arial; text-align: center; margin: 0; padding: 20px; background-color: #121212; color: #e0e0e0;}
    .container { max-width: 98vw; margin: 0 auto; background-color: #1e1e1e; padding: 20px; border-radius: 10px; box-shadow: 0 0 20px rgba(0,0,0,0.5); }
    .status-compact { display: flex; flex-wrap: wrap; gap: 10px; justify-content: center; font-size: 1rem; }
    .status-item { display: flex; align-items: center; gap: 5px; }
    .icon { font-size: 1.2rem; }
    .input-box { background-color: #2c2c2c; color: #e0e0e0; border: 1px solid #444; font-size: 1.2rem; padding: 8px; width: 100px; border-radius: 4px; }
    button { background-color: #3f51b5; border: none; color: white; padding: 10px 20px; text-align: center; font-size: 1.2rem; margin: 10px 0; cursor: pointer; border-radius: 4px; }
    button:hover { background-color: #303f9f; }
    .chart-container { margin-top: 30px; position: relative; height: 300px; }
    .heater-high { color: red; font-weight: bold; }
    .heater-medium { color: orange; font-weight: bold; }
    .heater-low { color: rgb(0, 140, 255); }
    h2 { color: white; }
    .settings { display: flex; justify-content: center; gap: 20px; margin: 20px 0; flex-wrap: wrap; }
    .settings div { text-align: left; }
    .settings input[type="checkbox"] {
      transform: scale(1.5);
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>Contrôle de Température - Tapis Chauffant</h2>
    <div class="status-compact">
      <div class="status-item"><span class="icon">🌡️</span> <span id="temperature">--</span>&deg;C</div>
      <div class="status-item"><span class="icon">💦</span> <span id="humidity">--</span>&#37;</div>
      <div class="status-item"><span class="icon">🔥</span> Tapis: <span id="heaterState">--</span>&#37;</div>
      <div class="status-item"><span class="icon">⏰</span> <span id="currentTime">--</span></div>
      <div class="status-item"><span class="icon">🌞</span> <span id="dayNightTransition">--</span></div>
      <div class="status-item"><span class="icon">📊</span> Moyenne: <span id="movingAverage">--</span>&deg;C</div>
      <div class="status-item"><span class="icon">🔥</span> Temp Max: <span id="maxTemperature">--</span>°C</div>
      <div class="status-item"><span class="icon">❄️</span> Temp Min: <span id="minTemperature">--</span>°C</div>
      <div id="weatherDisplay" class="status-item"><span class="icon">🌍</span> <span id="extWeather">-- °C / -- </span></div>
    </div>

  <div class="settings">
    <div><label for="dayTempSet">T jour:</label><input class="input-box" type="number" id="dayTempSet" min="5" max="35" value="28" step="0.5"></div>
    <div><label for="nightTempSet">T nuit:</label><input class="input-box" type="number" id="nightTempSet" min="5" max="35" value="22" step="0.5"></div>
    <div><label for="hysteresisSet">Hystérésis:</label><input class="input-box" type="number" id="hysteresisSet" min="0.1" max="5.0" value="0.3" step="0.1"></div>
    <div id="pwmSettings"><div><label for="KpSet">Kp:</label><input class="input-box" type="number" id="KpSet" min="0.1" max="10.0" value="2.0" step="0.1"></div>
    <div><label for="KiSet">Ki:</label><input class="input-box" type="number" id="KiSet" min="0.1" max="10.0" value="5.0" step="0.1"></div>
    <div><label for="KdSet">Kd:</label><input class="input-box" type="number" id="KdSet" min="0.1" max="10.0" value="1.0" step="0.1"></div>
    </div><div><label for="usePWM">PWM</label><input type="checkbox" id="usePWM"></div>
    <div id="weatherSettings"><div><label>Latitude:</label><input class="input-box" id="latInput" value="48.85"></div>
    <div><label>Longitude:</label><input class="input-box" id="lonInput" value="2.35"></div>
    </div><div><label>Météo</label><input type="checkbox" id="weatherMode"></div>
    <div><label>Caméra</label><input type="checkbox" id="showCamera"></div>

  </div>
  <div style="text-align: center; margin-top: 5px;">
    <button id="applyBtn"  style="font-size: 1rem; padding: 10px 25px;">Appliquer</button>
  </div>

<div id="cameraContainer" style="display: none; margin-top: 10px;">
  <img id="camStream" src="/capture" width="800" height="600" style="border:1px solid #ccc;">
</div>
    <div class="chart-container"><canvas id="tempChart"></canvas></div>
    <div class="chart-container"><canvas id="humidityChart"></canvas></div>
  </div>

<script>
const tempCtx = document.getElementById('tempChart').getContext('2d');
const humidityCtx = document.getElementById('humidityChart').getContext('2d');

const tempChart = new Chart(tempCtx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [{
      label: 'Température (°C)',
      data: [],
      borderColor: 'red',
      backgroundColor: 'rgba(255, 0, 0, 0.1)',
      borderWidth: 2,
      fill: true,
      tension: 0.3,
      pointRadius: 1,
      pointHoverRadius: 4
    }]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    scales: {
      y: {
        beginAtZero: false,
        ticks: {
          color: 'white'
        },
        title: {
          display: true,
          text: 'Température',
          color: 'white'
        }
      },
      x: {
        ticks: {
          color: 'white'
        },
        title: {
          display: true,
          text: 'Heure',
          color: 'white'
        }
      }
    },
    plugins: {
      legend: {
        labels: {
          color: 'white'
        },
        position: 'top'
      }
    }
  }
});

const humidityChart = new Chart(humidityCtx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [{
      label: 'Humidité',
      data: [],
      borderColor: 'cyan',
      backgroundColor: 'rgba(0, 255, 255, 0.1)',
      borderWidth: 2,
      fill: true,
      tension: 0.3,
      pointRadius: 1,
      pointHoverRadius: 4
    }]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    scales: {
      y: {
        beginAtZero: false,
        ticks: {
          color: 'white'
        },
        title: {
          display: true,
          text: 'Humidité',
          color: 'white'
        }
      },
      x: {
        ticks: {
          color: 'white'
        },
        title: {
          display: true,
          text: 'Heure',
          color: 'white'
        }
      }
    },
    plugins: {
      legend: {
        labels: {
          color: 'white'
        },
        position: 'top'
      }
    }
  }
});

// Charger l'historique à l'ouverture de la page
fetch("/history")
  .then(res => res.json())
  .then(data => {
    const timeLabels = data.map(entry => new Date(entry.t * 1000).toLocaleTimeString());
    const temperatures = data.map(entry => entry.temp);
    const humidities = data.map(entry => entry.hum);

    tempChart.data.labels = timeLabels;
    tempChart.data.datasets[0].data = temperatures;
    tempChart.update();

    humidityChart.data.labels = timeLabels;
    humidityChart.data.datasets[0].data = humidities;
    humidityChart.update();
  })
  .catch(err => console.error("Erreur chargement historique:", err));


function updateCameraVisibilityFromCheckbox() {
  const show = document.getElementById("showCamera").checked;
  document.getElementById("cameraContainer").style.display = show ? "block" : "none";

  // Sauvegarde dans l’ESP
  fetch("/setCamera?enabled=" + (show ? 1 : 0));

  // Stream par rafraîchissement
  if (show) {
    refreshInterval = setInterval(() => {
      const img = document.getElementById("camStream");
      img.src = "/capture?ts=" + new Date().getTime(); // éviter cache
    }, 200); ///////////////////////////////////////////////////////////////// 2 FPS
  } else {
    clearInterval(refreshInterval);
  }
}



function updateWeatherSettings() {
  const lat = document.getElementById("latInput").value;
  const lon = document.getElementById("lonInput").value;
  const enabled = document.getElementById("weatherMode").checked ? 1 : 0;
  fetch("/setWeather?lat="+lat+"&lon="+lon+"&enabled="+enabled)
    .then(res => res.text())
    .then(() => alert("Paramètres météo mis à jour !"))
    .catch(err => alert("Erreur mise à jour météo"));
}

function updateExternalWeather() {
console.log("→ Appel de updateExternalWeather");
fetch("/weatherData")
  .then(res => res.json())
  .then(data => {
    if (data.error) throw new Error("Réponse invalide");
    document.getElementById("extWeather").innerText = `${data.temp}°C / ${data.hum}%`;
  })
  .catch(err => {
    console.warn("❌ Erreur récupération météo", err);
    document.getElementById("extWeather").innerText = "-- °C / -- ";
  });

}

setInterval(updateExternalWeather, 60000); // toutes les minutes
updateExternalWeather(); // au chargement

function updatePWMMode() {
  var usePWM = document.getElementById("usePWM").checked ? 1 : 0;
  fetch("/setPWMMode?usePWM=" + usePWM)
    .then(response => response.text())
    .then(data => {
      alert("Mode PWM sauvegardé !");
      console.log("Mode PWM mis à jour :", data);
    })
    .catch(error => console.error("Erreur mise à jour PWM", error));
}

// Fonction de mise à jour des données
function updateData() {
  fetch("/temperature")
    .then(res => res.text())
    .then(data => {
      if (data !== "--") {
        const timeLabel = new Date().toLocaleTimeString();
        const tempValue = parseFloat(data);
        if (!isNaN(tempValue)) {
          tempChart.data.labels.push(timeLabel);
          tempChart.data.datasets[0].data.push(tempValue);
          if (tempChart.data.labels.length > 1000) {
            tempChart.data.labels.shift();
            tempChart.data.datasets[0].data.shift();
          }
          tempChart.update();
          document.getElementById("temperature").innerText = data;
        }
      }
    })
    .catch(err => console.error("Erreur lecture température:", err));

  fetch("/humidity")
    .then(res => res.text())
    .then(data => {
      if (data !== "--") {
        const timeLabel = new Date().toLocaleTimeString();
        const humidityValue = parseFloat(data);
        if (!isNaN(humidityValue)) {
          humidityChart.data.labels.push(timeLabel);
          humidityChart.data.datasets[0].data.push(humidityValue);
          if (humidityChart.data.labels.length > 1000) {
            humidityChart.data.labels.shift();
            humidityChart.data.datasets[0].data.shift();
          }
          humidityChart.update();
          document.getElementById("humidity").innerText = data;
        }
      }
    })
    .catch(err => console.error("Erreur lecture humidité:", err));

  fetch("/heaterState")
    .then(res => res.text())
    .then(data => {
      const element = document.getElementById("heaterState");
      element.innerText = data;

      const pwmValue = parseInt(data);
      if (pwmValue > 60) {
        element.className = "heater-high";
      } else if (pwmValue > 20 || (!document.getElementById("usePWM").checked && pwmValue > 0)) {
        element.className = "heater-medium";
      } else {
        element.className = "heater-low";
      }
    })
    .catch(err => console.error("Erreur lecture état chauffage:", err));

  fetch("/currentTime")
    .then(res => res.text())
    .then(data => {
      document.getElementById("currentTime").innerText = data;
    })
    .catch(err => console.error("Erreur lecture heure:", err));

  fetch("/dayNightTransition")
    .then(res => res.text())
    .then(data => {
      document.getElementById("dayNightTransition").innerText = data;
    })
    .catch(err => console.error("Erreur lecture cycle jour/nuit:", err));

  fetch("/movingAverage")
    .then(res => res.text())
    .then(data => {
      document.getElementById("movingAverage").innerText = data;
    })
    .catch(err => console.error("Erreur lecture moyenne:", err));

  fetch("/maxTemperature")
    .then(res => res.text())
    .then(data => {
      document.getElementById("maxTemperature").innerText = data;
    })
    .catch(err => console.error("Erreur lecture temp max:", err));

  fetch("/minTemperature")
    .then(res => res.text())
    .then(data => {
      document.getElementById("minTemperature").innerText = data;
    })
    .catch(err => console.error("Erreur lecture temp min:", err));
}

// Mise à jour initiale et périodique des données
updateData();
setInterval(updateData, 5000);

// Initialisation des valeurs des champs
document.addEventListener('DOMContentLoaded', () => {

    fetch("/temperature").then(res => res.text()).then(data => {
      if (data !== "--") document.getElementById("temperature").innerText = data;
    });
    
    fetch("/humidity").then(res => res.text()).then(data => {
      if (data !== "--") document.getElementById("humidity").innerText = data;
    });
      
    // Récupérer les paramètres actuels

  document.getElementById("usePWM").addEventListener("change", updateVisibility);
  document.getElementById("weatherMode").addEventListener("change", updateVisibility);
  document.getElementById("showCamera").addEventListener("change", updateCameraVisibility);

  fetch("/getSettings")
    .then(res => res.json())
    .then(settings => {
      if (settings.dayTemp !== undefined) document.getElementById("dayTempSet").value = settings.dayTemp;
      if (settings.nightTemp !== undefined) document.getElementById("nightTempSet").value = settings.nightTemp;
      if (settings.hysteresis !== undefined) document.getElementById("hysteresisSet").value = settings.hysteresis;
      if (settings.Kp !== undefined) document.getElementById("KpSet").value = settings.Kp;
      if (settings.Ki !== undefined) document.getElementById("KiSet").value = settings.Ki;
      if (settings.Kd !== undefined) document.getElementById("KdSet").value = settings.Kd;
      if (settings.weatherMode !== undefined) document.getElementById("weatherMode").checked = settings.weatherMode;
      if (settings.usePWM !== undefined) document.getElementById("usePWM").checked = settings.usePWM;
      if (settings.latitude !== undefined) document.getElementById("latInput").value = settings.latitude;
      if (settings.longitude !== undefined) document.getElementById("lonInput").value = settings.longitude;
      
      // ✅ Caméra
      const enabled = settings.cameraEnabled || false;
      document.getElementById("showCamera").checked = enabled;
      cameraEnabled = enabled;

      updateVisibility();            // gère affichage météo et pwm
      updateCameraVisibility();      // démarre le rafraîchissement image
    })
    .catch(err => console.error("Erreur récupération paramètres:", err));
});

function setTemperature() {
  const dayTemp = document.getElementById("dayTempSet").value;
  const nightTemp = document.getElementById("nightTempSet").value;
  const hysteresis = document.getElementById("hysteresisSet").value;
  const Kp = document.getElementById("KpSet").value;
  const Ki = document.getElementById("KiSet").value;
  const Kd = document.getElementById("KdSet").value;

  // Mise à jour des températures
  
  fetch("/setTemp?dayTemp=" + dayTemp + "&nightTemp=" + nightTemp)
    .then(response => {
      if (!response.ok) throw new Error("Erreur serveur");
      return response.text();
    })
    .then(() => console.log("Températures sauvegardées !"))
    .catch(error => console.error("Erreur lors de la sauvegarde des températures:", error));

  // Mise à jour de l'hystérésis
  
  fetch("/setHysteresis?hysteresis=" + hysteresis )
    .then(response => {
      if (!response.ok) throw new Error("Erreur serveur");
      return response.text();
    })
    .then(() => console.log("Hystérésis sauvegardée !"))
    .catch(error => console.error("Erreur lors de la sauvegarde de l'hystérésis:", error));

  // Mise à jour des paramètres PID
  fetch("/setPID?Kp=" + Kp +"&Ki=" + Ki +"&Kd=" + Kd)
    .then(response => {
      if (!response.ok) throw new Error("Erreur serveur");
      return response.text();
    })
    .then(() => console.log("Paramètres PID sauvegardés !"))
    .catch(error => console.error("Erreur lors de la sauvegarde des PID:", error));
    
  alert("Paramètres sauvegardés !");
}

function applyAllSettings() {
  // Paramètres de température et PID
  const dayTemp = document.getElementById("dayTempSet").value;
  const nightTemp = document.getElementById("nightTempSet").value;
  const hysteresis = document.getElementById("hysteresisSet").value;
  const Kp = document.getElementById("KpSet").value;
  const Ki = document.getElementById("KiSet").value;
  const Kd = document.getElementById("KdSet").value;
  
  // Paramètres PWM
  const usePWM = document.getElementById("usePWM").checked ? 1 : 0;
  
  // Paramètres météo
  const lat = document.getElementById("latInput").value;
  const lon = document.getElementById("lonInput").value;
  const weatherEnabled = document.getElementById("weatherMode").checked ? 1 : 0;
  //cam !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  const showCamera = document.getElementById("showCamera").checked ? 1 : 0;
  // Mise à jour de tous les paramètres en séquence
  Promise.all([
    // Températures
    fetch("/setTemp?dayTemp="+dayTemp+"&nightTemp="+nightTemp)
      .then(response => {
        if (!response.ok) throw new Error("Erreur serveur pour température");
        return response.text();
      }),
      
    // Hystérésis  
    fetch("/setHysteresis?hysteresis="+hysteresis)
      .then(response => {
        if (!response.ok) throw new Error("Erreur serveur pour hystérésis");
        return response.text();
      }),
      
    // PID
    fetch("/setPID?Kp="+Kp+"&Ki="+Ki+"&Kd="+Kd)
      .then(response => {
        if (!response.ok) throw new Error("Erreur serveur pour PID");
        return response.text();
      }),
      
    // PWM
    fetch("/setPWMMode?usePWM="+usePWM)
      .then(response => {
        if (!response.ok) throw new Error("Erreur serveur pour PWM");
        return response.text();
      }),
      
    // Météo
    fetch("/setWeather?lat="+lat+"&lon="+lon+"&enabled="+weatherEnabled)
      .then(response => {
        if (!response.ok) throw new Error("Erreur serveur pour météo");
        return response.text();
      })
  ])
  .then(() => {
    alert("Tous les paramètres ont été sauvegardés avec succès!");
    console.log("Paramètres mis à jour");
  })
  .catch(error => {
    alert("Erreur lors de la sauvegarde des paramètres: " + error.message);
    console.error("Erreur:", error);
  });
}
document.getElementById("applyBtn").addEventListener("click", applyAllSettings);
</script>










</body>
</html>


)rawliteral";