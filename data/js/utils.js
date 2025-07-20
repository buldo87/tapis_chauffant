/**
 * @file utils.js
 * @description Fonctions utilitaires partagées.
 */

/**
 * Parse les données de température annuelles depuis un ArrayBuffer.
 * @param {ArrayBuffer} arrayBuffer - Le buffer contenant les données binaires.
 * @returns {Promise<Array<Array<number>>>} Un tableau 2D des températures.
 */
export async function parseBinData(arrayBuffer) {
    const yearlyData = [];
    const view = new DataView(arrayBuffer);
    // Chaque jour a 24 températures, chaque température est un int16 (2 octets)
    const numDays = arrayBuffer.byteLength / (24 * 2);

    for (let day = 0; day < numDays; day++) {
        const dayData = [];
        for (let hour = 0; hour < 24; hour++) {
            const byteOffset = (day * 24 + hour) * 2;
            // Les données sont stockées en little-endian sur l'ESP32
            const temp = view.getInt16(byteOffset, true);
            dayData.push(temp);
        }
        yearlyData.push(dayData);
    }
    return yearlyData;
}