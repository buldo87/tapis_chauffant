import requests
import json
from datetime import datetime, timedelta
import time
import os

# --- Configuration ---
LATITUDE = 46.1829
LONGITUDE = 2.1094
START_YEAR = 2020
END_YEAR = 2024
OUTPUT_FILE = "seasonal_data_averaged.json"
CACHE_DIR = "weather_cache"
# -------------------

def get_average_yearly_data():
    """
    Fetches historical weather data for a range of years using one request per year,
    calculates the hourly average for each day of the year, and saves it to a file.
    It uses a local cache to avoid re-downloading data.
    """
    if not os.path.exists(CACHE_DIR):
        os.makedirs(CACHE_DIR)
        print(f"Created cache directory: {CACHE_DIR}")

    # Structure to hold all temperature readings: day -> hour -> [temps]
    all_temps = [[[] for _ in range(24)] for _ in range(366)]

    print(f"Starting data acquisition for years {START_YEAR} to {END_YEAR}...")

    for year in range(START_YEAR, END_YEAR + 1):
        print(f"\nProcessing Year: {year}")
        cache_file = os.path.join(CACHE_DIR, f"yearly_data_{year}.json")
        yearly_hourly_data = None

        # 1. Check cache first
        if os.path.exists(cache_file):
            try:
                with open(cache_file, 'r') as f:
                    yearly_hourly_data = json.load(f)
                print(f"  - Year {year}: Loaded from cache")
            except json.JSONDecodeError:
                print(f"  - Year {year}: Invalid cache file. Re-fetching...")
                yearly_hourly_data = None

        # 2. If not in cache, fetch from API
        if yearly_hourly_data is None:
            start_date = f"{year}-01-01"
            end_date = f"{year}-12-31"
            api_url = (
                f"https://archive-api.open-meteo.com/v1/archive?"
                f"latitude={LATITUDE}&longitude={LONGITUDE}&"
                f"start_date={start_date}&end_date={end_date}&"
                f"hourly=temperature_2m"
            )
            print(f"  - Year {year}: Fetching from API...")
            print(f"    URL: {api_url}")
            try:
                response = requests.get(api_url, timeout=60) # Increased timeout for large request
                response.raise_for_status()
                api_data = response.json()
                yearly_hourly_data = api_data.get("hourly", {})
                
                # Save to cache
                with open(cache_file, 'w') as f:
                    json.dump(yearly_hourly_data, f)
                print(f"  - Year {year}: Fetched and cached.")
                time.sleep(1) # Be nice to the API

            except requests.exceptions.RequestException as e:
                print(f"  - Year {year}: [ERROR] API request failed: {e}")
                continue # Skip to next year

        # 3. Add fetched data to our main structure
        time_data = yearly_hourly_data.get("time", [])
        temp_data = yearly_hourly_data.get("temperature_2m", [])

        if not time_data or not temp_data or len(time_data) != len(temp_data):
            print(f"  - Year {year}: [WARN] Incomplete or mismatched data found.")
            continue

        for i, dt_str in enumerate(time_data):
            try:
                # Parse datetime to get day of year
                dt_obj = datetime.fromisoformat(dt_str)
                day_of_year_idx = dt_obj.timetuple().tm_yday - 1 # tm_yday is 1-based
                hour = dt_obj.hour
                temp = temp_data[i]

                if temp is not None:
                    all_temps[day_of_year_idx][hour].append(temp)
            except (ValueError, IndexError) as e:
                print(f"  - [WARN] Could not process entry: {dt_str} - {e}")


    # --- Calculate Averages ---
    print("\nAll data collected. Calculating averages...")
    final_yearly_temps = [[20.0 for _ in range(24)] for _ in range(366)]

    for day in range(366):
        for hour in range(24):
            readings = all_temps[day][hour]
            if readings:
                average = sum(readings) / len(readings)
                final_yearly_temps[day][hour] = round(average, 1)
            else:
                # Use previous hour's data or a default if no data was ever collected
                if hour > 0:
                    final_yearly_temps[day][hour] = final_yearly_temps[day][hour-1]
                elif day > 0: # for hour 0, use previous day's last hour
                     final_yearly_temps[day][hour] = final_yearly_temps[day-1][23]
                else: # for day 0, hour 0
                    final_yearly_temps[day][hour] = 20.0


    # --- Save Final File ---
    print(f"Averaging complete. Saving to {OUTPUT_FILE}...")
    with open(OUTPUT_FILE, 'w') as f:
        json.dump(final_yearly_temps, f)

    print(f"✅ Done! Averaged data saved to '{OUTPUT_FILE}'. You can now upload this file to the ESP32.")

if __name__ == "__main__":
    get_average_yearly_data()