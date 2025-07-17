/**
 * @file ui/tabs.js
 * @description Gère la navigation par onglets entre "Surveillance" et "Configuration".
 */

const tabButtons = document.querySelectorAll('.tab-button');
const dashboardContent = document.getElementById('dashboardContent');
const configContent = document.getElementById('configContent');

export function initTabs(onTabChange) {
    tabButtons.forEach(button => {
        button.addEventListener('click', () => {
            const newTab = button.id === 'tabDashboard' ? 'dashboard' : 'config';

            // Mettre à jour l'état des boutons
            tabButtons.forEach(btn => btn.classList.replace('tab-active', 'tab-inactive'));
            button.classList.replace('tab-inactive', 'tab-active');
            
            // Afficher le contenu correspondant
            dashboardContent.style.display = (newTab === 'dashboard') ? 'block' : 'none';
            configContent.style.display = (newTab === 'config') ? 'block' : 'none';
            
            // Appeler le callback pour notifier le changement
            if (typeof onTabChange === 'function') {
                onTabChange(newTab);
            }
        });
    });
}