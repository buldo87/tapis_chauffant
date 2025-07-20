
import { api } from '../api.js';

const elements = {
    container: document.getElementById('cameraContainer'),
    stream: document.getElementById('camStream'),
    error: document.getElementById('cameraError'),
    retryBtn: document.getElementById('retryCameraBtn'),
    toggle: document.getElementById('showCamera'),
    resolutionSelect: document.getElementById('cameraResolution')
};

function updateVisibility(enabled) {
    if (!elements.container) return;
    elements.container.style.display = enabled ? 'block' : 'none';
}

function startStream() {
    if (!elements.stream) return;
    console.log("📹 Démarrage du stream caméra...");
    elements.error.style.display = 'none';
    elements.stream.src = '/mjpeg?' + new Date().getTime();
}

function stopStream() {
    if (!elements.stream) return;
    console.log("🛑 Arrêt du stream caméra.");
    elements.stream.src = '';
}

async function setCameraEnabled(enabled) {
    try {
        // This is a fire-and-forget call for now
        fetch(`/setCamera?enabled=${enabled ? 1 : 0}`);
    } catch (error) {
        console.error("Failed to set camera state:", error);
    }
}

export function initCamera(config) {
    if (!elements.toggle) return;

    elements.toggle.checked = config.cameraEnabled;
    updateVisibility(config.cameraEnabled);
    if (config.cameraEnabled) {
        startStream();
    }

    elements.toggle.addEventListener('change', (e) => {
        const enabled = e.target.checked;
        setCameraEnabled(enabled);
        updateVisibility(enabled);
        if (enabled) {
            startStream();
        } else {
            stopStream();
        }
    });

    elements.stream.onerror = () => {
        console.error("❌ Erreur de connexion au stream caméra.");
        elements.error.style.display = 'block';
        stopStream();
    };

    elements.stream.onload = () => {
        console.log("✅ Stream caméra connecté.");
        elements.error.style.display = 'none';
    };

    elements.retryBtn.addEventListener('click', startStream);

    elements.resolutionSelect.addEventListener('change', (e) => {
        const resolution = e.target.value;
        fetch(`/set-resolution-cam?quality=${resolution}`);
    });
}
