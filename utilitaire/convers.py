import json
import struct

def generate_temperature_bin(json_file, bin_file):
    # Lire les données de température à partir du fichier JSON
    with open(json_file, 'r') as f:
        seasonal_data = json.load(f)

    # Ouvrir le fichier binaire pour l'écriture
    with open(bin_file, 'wb') as f:
        # Boucle à travers chaque jour et chaque heure
        for day in seasonal_data:
            for temp in day:
                # Convertir la température en entier (décimale -> entier)
                temp_int = int(round(temp * 10))  # Multiplier par 10 et arrondir
                # Écrire l'entier dans le fichier binaire (format little-endian)
                f.write(struct.pack('<h', temp_int))  # 'h' pour int16

    print(f"Fichier {bin_file} généré avec succès!")

# Nom des fichiers
json_input_file = 'seasonal_data_averaged.json'  # Remplacez par le chemin de votre fichier JSON
bin_output_file = 'temperature.bin'

# Générer le fichier binaire
generate_temperature_bin(json_input_file, bin_output_file)
