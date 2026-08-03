#pragma once
// Vorlage -> nach include/user_config.h kopieren und mit eigenen Werten fuellen.

// --- SwitchBot-Sensoren (deviceId = MAC ohne Doppelpunkte, Grossbuchstaben) ---
#define DEVICE_COUNT 4
static const char* const DEVICE_IDS[DEVICE_COUNT] =
  {"AAAAAAAAAAAA","BBBBBBBBBBBB","CCCCCCCCCCCC","DDDDDDDDDDDD"};
static const char* const DEVICE_NAMES[DEVICE_COUNT] =
  {"Aussen Hinten","Aussen Vorne","Büro","Küche"};
static const bool DEVICE_OUTDOOR[DEVICE_COUNT] = {true,true,false,false};

// --- Standort fuer Open-Meteo-Wettervorhersage (eigene Koordinaten) ---
#define FORECAST_LAT  "48.0000"
#define FORECAST_LON  "16.0000"
#define FORECAST_TZ   "Europe/Vienna"

// --- Anzeige ---
// 0 = 4 Thermometer + Tages-Vorschau, 1 = 4 Thermometer + Stunden-Vorschau,
// 2 = 6 Thermometer (2x3) ohne Wetterleiste
#define DISPLAY_MODE 0
