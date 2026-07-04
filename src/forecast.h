#pragma once

enum WIcon { ICON_SUN, ICON_PARTLY, ICON_CLOUD, ICON_RAIN, ICON_SNOW, ICON_STORM };

struct DayForecast {
  int  wmoCode;   // Open-Meteo WMO weather code
  int  tMax;      // gerundet °C
  int  tMin;      // gerundet °C
  int  wday;      // 0=So .. 6=Sa
  bool valid;
};

struct HourForecast {
  int   hour;      // 0..23 (lokale Zeit lt. API-Timezone)
  int   wmoCode;   // Open-Meteo WMO weather code
  int   temp;      // gerundet °C
  float precipMm;  // Niederschlag mm in dieser Stunde
  bool  valid;
};

// WMO-Wettercode -> Icon-Kategorie
WIcon wmoToIcon(int code);

// Wochentag (0=So..6=Sa) aus Datum, Sakamoto-Algorithmus
int weekdayFromDate(int y, int m, int d);

// Parst Open-Meteo /v1/forecast (daily). Fuellt bis maxDays Eintraege,
// schreibt Anzahl nach *outCount. Gibt false bei Fehler.
bool parseForecastJson(const char* json, DayForecast* out, int maxDays, int* outCount);

// Geraet: holt die Vorhersage per HTTPS. Gibt Anzahl gueltiger Tage zurueck.
int fetchForecast(DayForecast* out, int maxDays);

// Parst Open-Meteo /v1/forecast (hourly). Fuellt bis maxHours Eintraege.
bool parseHourlyJson(const char* json, HourForecast* out, int maxHours, int* outCount);

// Geraet: holt die naechsten Stunden per HTTPS (forecast_hours). Anzahl zurueck.
int fetchHourlyForecast(HourForecast* out, int maxHours);
