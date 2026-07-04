#include "forecast.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

WIcon wmoToIcon(int c) {
  if (c == 0) return ICON_SUN;
  if (c == 1 || c == 2) return ICON_PARTLY;
  if (c == 45 || c == 48) return ICON_FOG;
  if (c == 3) return ICON_CLOUD;
  if (c == 71 || c == 73 || c == 75 || c == 77 || c == 85 || c == 86) return ICON_SNOW;
  if (c == 95 || c == 96 || c == 99) return ICON_STORM;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return ICON_RAIN;
  return ICON_CLOUD;
}

// Tag/Nacht-Variante: nachts wird aus Sonne Mond (0..1) bzw. Mond+Wolke (2).
WIcon wmoToIconDN(int c, bool isDay) {
  if (!isDay) {
    if (c == 0 || c == 1) return ICON_MOON;
    if (c == 2) return ICON_MOON_PARTLY;
  }
  return wmoToIcon(c);
}

int weekdayFromDate(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;  // 0=So
}

bool parseForecastJson(const char* json, DayForecast* out, int maxDays, int* outCount) {
  if (outCount) *outCount = 0;
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonObject daily = doc["daily"];
  if (daily.isNull()) return false;
  JsonArray time = daily["time"];
  JsonArray code = daily["weather_code"];
  JsonArray tmax = daily["temperature_2m_max"];
  JsonArray tmin = daily["temperature_2m_min"];
  if (time.isNull() || code.isNull() || tmax.isNull() || tmin.isNull()) return false;
  int n = 0;
  for (int i = 0; i < (int)time.size() && n < maxDays; i++) {
    const char* date = time[i];           // "YYYY-MM-DD"
    int y = 0, m = 0, d = 0;
    if (date) { y = atoi(date); m = atoi(date + 5); d = atoi(date + 8); }
    out[n].wmoCode = code[i].as<int>();
    out[n].tMax = (int)lround(tmax[i].as<float>());
    out[n].tMin = (int)lround(tmin[i].as<float>());
    out[n].wday = (y && m && d) ? weekdayFromDate(y, m, d) : 0;
    out[n].valid = true;
    n++;
  }
  if (outCount) *outCount = n;
  return n > 0;
}

bool parseHourlyJson(const char* json, HourForecast* out, int maxHours, int* outCount) {
  if (outCount) *outCount = 0;
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonObject hourly = doc["hourly"];
  if (hourly.isNull()) return false;
  JsonArray time = hourly["time"];
  JsonArray code = hourly["weather_code"];
  JsonArray temp = hourly["temperature_2m"];
  JsonArray prec = hourly["precipitation"];
  JsonArray isday = hourly["is_day"];   // optional (aeltere Antworten)
  if (time.isNull() || code.isNull() || temp.isNull() || prec.isNull()) return false;
  int n = 0;
  for (int i = 0; i < (int)time.size() && n < maxHours; i++) {
    const char* ts = time[i];              // "YYYY-MM-DDTHH:MM"
    out[n].hour = (ts && strlen(ts) >= 13) ? atoi(ts + 11) : 0;
    out[n].wmoCode = code[i].as<int>();
    out[n].temp = (int)lround(temp[i].as<float>());
    out[n].precipMm = prec[i].as<float>();
    out[n].isDay = isday.isNull() ? true : (isday[i].as<int>() != 0);
    out[n].valid = true;
    n++;
  }
  if (outCount) *outCount = n;
  return n > 0;
}

#if defined(ARDUINO)
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"

int fetchForecast(DayForecast* out, int maxDays) {
  for (int i = 0; i < maxDays; i++) out[i].valid = false;
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + FORECAST_LAT +
               "&longitude=" + FORECAST_LON +
               "&daily=weather_code,temperature_2m_max,temperature_2m_min" +
               "&timezone=" + FORECAST_TZ + "&forecast_days=" + String(maxDays);
  if (!http.begin(cli, url)) return 0;
  int count = 0;
  int httpCode = http.GET();
  if (httpCode == 200) {
    String body = http.getString();
    parseForecastJson(body.c_str(), out, maxDays, &count);
  } else {
    Serial.printf("Open-Meteo HTTP %d\n", httpCode);
  }
  http.end();
  return count;
}

int fetchHourlyForecast(HourForecast* out, int maxHours) {
  for (int i = 0; i < maxHours; i++) out[i].valid = false;
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + FORECAST_LAT +
               "&longitude=" + FORECAST_LON +
               "&hourly=temperature_2m,weather_code,precipitation,is_day" +
               "&timezone=" + FORECAST_TZ + "&forecast_hours=" + String(maxHours);
  if (!http.begin(cli, url)) return 0;
  int count = 0;
  int httpCode = http.GET();
  if (httpCode == 200) {
    String body = http.getString();
    parseHourlyJson(body.c_str(), out, maxHours, &count);
  } else {
    Serial.printf("Open-Meteo hourly HTTP %d\n", httpCode);
  }
  http.end();
  return count;
}
#else
int fetchForecast(DayForecast*, int) { return 0; }
int fetchHourlyForecast(HourForecast*, int) { return 0; }
#endif
