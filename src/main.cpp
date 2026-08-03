// Endfassung: ESP32-S3-PhotoPainter SwitchBot Wetter.
// Nur im [env:photopainter] gebaut (build_src_filter schliesst *test/scan aus).
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "config.h"
#include "secrets.h"
#include "reading.h"
#include "power_logic.h"
#include "switchbot_api.h"
#include "forecast.h"
#include "local_sensors.h"
#include "display_view.h"

// Serielle Diagnose nur im Debug-Build (dort ist USB-CDC an).
#if ARDUINO_USB_CDC_ON_BOOT
#define LOGF(...) Serial.printf(__VA_ARGS__)
#else
#define LOGF(...) do {} while (0)
#endif

// Zeitzone Deutschland (CET/CEST mit automatischer Sommerzeit)
static const char* TZ_DE = "CET-1CEST,M3.5.0,M10.5.0/3";

RTC_DATA_ATTR SensorReading g_prev[DEVICE_COUNT];
RTC_DATA_ATTR bool g_havePrev = false;
RTC_DATA_ATTR int  g_ntpDay   = -1;   // letzter NTP-Sync (tm_yday)
RTC_DATA_ATTR DayForecast g_prevFc[FORECAST_DAYS];
RTC_DATA_ATTR int  g_prevFcCount = 0;
RTC_DATA_ATTR HourForecast g_prevHf[FORECAST_HOURS];
RTC_DATA_ATTR int  g_prevHfCount = 0;
RTC_DATA_ATTR uint32_t g_lastFcFetch = 0;     // time() der letzten Vorhersage
RTC_DATA_ATTR uint32_t g_lastDraw    = 0;     // time() des letzten Panel-Refresh

static bool wifiWait(uint32_t ms) {
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < ms) delay(50);
  return WiFi.status() == WL_CONNECTED;
}

// Kein BSSID/Kanal-Cache aus dem RTC-RAM: als Stromsparmassnahme am
// 31.07.2026 eingebaut, griff aber erstmals im zweiten Zyklus -- genau dort,
// wo der Aufhaenger auftrat. Bis zur Klaerung immer normal verbinden.
static bool wifiConnect(uint32_t ms = 15000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  return wifiWait(ms);
}

static void maybeNtp(struct tm& now, bool haveTime) {
  // Einmal taeglich -- und immer dann, wenn die RTC ihre Zeit verloren hat
  // (z.B. weil ihre Versorgungsschiene im Schlaf abgeschaltet war).
  if (haveTime && !rtcLostPower() && g_ntpDay == now.tm_yday) return;
  configTzTime(TZ_DE, "pool.ntp.org", "time.nist.gov");
  struct tm t;
  if (getLocalTime(&t, 8000)) {
    rtcSet(t);
    g_ntpDay = t.tm_yday;
    now = t;
  }
}

static void sleepFor(uint32_t sec) {
#ifdef DEBUG_SLEEP_S
  sec = DEBUG_SLEEP_S;   // Debug: kurzer Zyklus
#endif
  localSleepPrepare();   // nicht benoetigte Versorgungsschienen abschalten
  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.begin(115200);
  delay(300);
#endif
  localInit();

  // 1) lokale Werte + Zeit
  HeaderInfo hi{};
  struct tm now{};
  readSHTC3(hi.localTemp, hi.localHum);
  readBattery(hi.battPct, hi.charging);
  bool haveTime = rtcNow(now);
  bool rtcReset = rtcLostPower();

  // 2) WLAN + ggf. NTP + SwitchBot-Sensoren + Wettervorhersage
  SensorReading cur[DEVICE_COUNT];
  DayForecast fc[FORECAST_DAYS];
  int fcCount = 0;
  HourForecast hf[FORECAST_HOURS];
  int hfCount = 0;
  hi.wifiOk = wifiConnect();
  if (hi.wifiOk) {
    maybeNtp(now, haveTime);
    haveTime = rtcNow(now);
    fetchAll(DEVICE_IDS, cur, DEVICE_COUNT);
#if DISPLAY_MODE == 0 || DISPLAY_MODE == 1
    // Die Vorhersage aendert sich nicht im Update-Takt -> seltener holen.
    uint32_t tNow = (uint32_t)time(nullptr);
    bool fcDue = (g_lastFcFetch == 0) || (tNow < g_lastFcFetch) ||
                 (tNow - g_lastFcFetch >= (uint32_t)FORECAST_INTERVAL_S);
#if DISPLAY_MODE == 0
    if (fcDue || g_prevFcCount == 0) {
      fcCount = fetchForecast(fc, FORECAST_DAYS);
      if (fcCount > 0) g_lastFcFetch = tNow;
    }
#else
    if (fcDue || g_prevHfCount == 0) {
      hfCount = fetchHourlyForecast(hf, FORECAST_HOURS);
      if (hfCount > 0) g_lastFcFetch = tNow;
    }
#endif
#endif
  } else {
    for (int i = 0; i < DEVICE_COUNT; i++) {
      strncpy(cur[i].id, DEVICE_IDS[i], sizeof(cur[i].id) - 1);
      cur[i].valid = false;
    }
  }
  hi.hour = haveTime ? now.tm_hour : 12;
  hi.minute = haveTime ? now.tm_min : 0;

  // 3) Fehlende Sensoren / Vorhersage -> letzten bekannten Stand behalten
  if (g_havePrev)
    for (int i = 0; i < DEVICE_COUNT; i++)
      if (!cur[i].valid && g_prev[i].valid) cur[i] = g_prev[i];
  if (fcCount == 0 && g_prevFcCount > 0) {
    for (int i = 0; i < g_prevFcCount; i++) fc[i] = g_prevFc[i];
    fcCount = g_prevFcCount;
  }
  if (hfCount == 0 && g_prevHfCount > 0) {
    for (int i = 0; i < g_prevHfCount; i++) hf[i] = g_prevHf[i];
    hfCount = g_prevHfCount;
  }

  // 4) Nur bei Aenderung zeichnen (Sensoren ODER Vorhersage), spaetestens
  //    aber nach FORCE_REDRAW_MIN, damit die Uhrzeit im Kopf aktuell bleibt.
  bool fcChanged = (fcCount != g_prevFcCount);
  for (int i = 0; i < fcCount && !fcChanged; i++)
    if (fc[i].wmoCode != g_prevFc[i].wmoCode ||
        fc[i].tMax != g_prevFc[i].tMax || fc[i].tMin != g_prevFc[i].tMin)
      fcChanged = true;
  bool hfChanged = (hfCount != g_prevHfCount);
  for (int i = 0; i < hfCount && !hfChanged; i++)
    if (hf[i].hour != g_prevHf[i].hour || hf[i].wmoCode != g_prevHf[i].wmoCode ||
        hf[i].temp != g_prevHf[i].temp ||
        hf[i].isDay != g_prevHf[i].isDay ||
        fabsf(hf[i].precipMm - g_prevHf[i].precipMm) > 0.05f)
      hfChanged = true;
  uint32_t tDraw = (uint32_t)time(nullptr);
  bool stale = (FORCE_REDRAW_MIN > 0) && g_lastDraw != 0 &&
               (tDraw < g_lastDraw ||
                tDraw - g_lastDraw >= (uint32_t)FORCE_REDRAW_MIN * 60u);
  bool changed = !g_havePrev || anyChanged(cur, g_prev, DEVICE_COUNT) ||
                 fcChanged || hfChanged || stale;
  LOGF("wifi=%d time=%02d:%02d changed=%d stale=%d rtcReset=%d fc=%d sleep=%us\n",
       hi.wifiOk, hi.hour, hi.minute, changed, stale, rtcReset, fcCount,
       sleepSeconds(hi.hour, hi.battPct, hi.charging));
  LOGF("local SHTC3: %.1fC %d%%  Akku %d%% chg=%d\n",
       hi.localTemp, hi.localHum, hi.battPct, hi.charging);
#if ARDUINO_USB_CDC_ON_BOOT
  for (int i = 0; i < DEVICE_COUNT; i++)
    LOGF("  [%s] %s: valid=%d %.1fC %d%% batt=%d\n",
         DEVICE_IDS[i], DEVICE_NAMES[i], cur[i].valid,
         cur[i].temperature, cur[i].humidity, cur[i].battery);
#else
  (void)rtcReset;
#endif

  // 5) Funk aus, bevor gezeichnet wird: der Refresh dauert ~30 s ohne Netz,
  //    und der Light Sleep im BUSY-Warten vertraegt sich nicht mit aktivem WLAN.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  if (changed) {
    displayInit();
    displayRender(cur, DEVICE_COUNT, hi, fc, fcCount, hf, hfCount);
    g_lastDraw = (uint32_t)time(nullptr);
    // Nur was wirklich auf dem Panel steht, wird zur Vergleichsbasis. Sonst
    // wuerde sich die Anzeige in 0,1-K-Schritten unbemerkt von der Realitaet
    // wegschleichen, ohne je die Refresh-Schwelle zu reissen.
    for (int i = 0; i < DEVICE_COUNT; i++) g_prev[i] = cur[i];
    g_havePrev = true;
    for (int i = 0; i < fcCount; i++) g_prevFc[i] = fc[i];
    g_prevFcCount = fcCount;
    for (int i = 0; i < hfCount; i++) g_prevHf[i] = hf[i];
    g_prevHfCount = hfCount;
  }

  // 6) Schlafen
  sleepFor(sleepSeconds(hi.hour, hi.battPct, hi.charging));
}

void loop() {}
