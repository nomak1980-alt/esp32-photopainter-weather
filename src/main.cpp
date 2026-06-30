// Endfassung: ESP32-S3-PhotoPainter SwitchBot Wetter.
// Nur im [env:photopainter] gebaut (build_src_filter schliesst *test/scan aus).
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "secrets.h"
#include "reading.h"
#include "power_logic.h"
#include "switchbot_api.h"
#include "local_sensors.h"
#include "display_view.h"

// Zeitzone Deutschland (CET/CEST mit automatischer Sommerzeit)
static const char* TZ_DE = "CET-1CEST,M3.5.0,M10.5.0/3";

RTC_DATA_ATTR SensorReading g_prev[DEVICE_COUNT];
RTC_DATA_ATTR bool g_havePrev = false;
RTC_DATA_ATTR int  g_ntpDay   = -1;   // letzter NTP-Sync (tm_yday)

static bool wifiConnect(uint32_t ms = 15000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < ms) delay(200);
  return WiFi.status() == WL_CONNECTED;
}

static void maybeNtp(struct tm& now) {
  if (g_ntpDay == now.tm_yday) return;        // heute schon gesynct
  configTzTime(TZ_DE, "pool.ntp.org", "time.nist.gov");
  struct tm t;
  if (getLocalTime(&t, 8000)) {
    rtcSet(t);
    g_ntpDay = t.tm_yday;
    now = t;
  }
}

static void sleepFor(uint32_t sec) {
  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  localInit();

  // 1) lokale Werte + Zeit
  HeaderInfo hi{};
  struct tm now{};
  readSHTC3(hi.localTemp, hi.localHum);
  readBattery(hi.battPct, hi.charging);
  bool haveTime = rtcNow(now);

  // 2) WLAN + ggf. NTP + SwitchBot-Sensoren
  SensorReading cur[DEVICE_COUNT];
  hi.wifiOk = wifiConnect();
  if (hi.wifiOk) {
    maybeNtp(now);
    haveTime = rtcNow(now);
    fetchAll(DEVICE_IDS, cur, DEVICE_COUNT);
  } else {
    for (int i = 0; i < DEVICE_COUNT; i++) {
      strncpy(cur[i].id, DEVICE_IDS[i], sizeof(cur[i].id) - 1);
      cur[i].valid = false;
    }
  }
  hi.hour = haveTime ? now.tm_hour : 12;
  hi.minute = haveTime ? now.tm_min : 0;

  // 3) Fehlende Sensoren -> letzten bekannten Stand behalten
  if (g_havePrev)
    for (int i = 0; i < DEVICE_COUNT; i++)
      if (!cur[i].valid && g_prev[i].valid) cur[i] = g_prev[i];

  // 4) Nur bei Aenderung zeichnen
  bool changed = !g_havePrev || anyChanged(cur, g_prev, DEVICE_COUNT);
  Serial.printf("wifi=%d time=%02d:%02d changed=%d sleep=%us\n",
                hi.wifiOk, hi.hour, hi.minute, changed, sleepSeconds(hi.hour));
  Serial.printf("local SHTC3: %.1fC %d%%  Akku %d%% chg=%d\n",
                hi.localTemp, hi.localHum, hi.battPct, hi.charging);
  for (int i = 0; i < DEVICE_COUNT; i++)
    Serial.printf("  [%s] %s: valid=%d %.1fC %d%% batt=%d\n",
                  DEVICE_IDS[i], DEVICE_NAMES[i], cur[i].valid,
                  cur[i].temperature, cur[i].humidity, cur[i].battery);
  if (changed) {
    displayInit();
    displayRender(cur, DEVICE_COUNT, hi);
  }

  // 5) Stand sichern, Funk aus, schlafen
  for (int i = 0; i < DEVICE_COUNT; i++) g_prev[i] = cur[i];
  g_havePrev = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  sleepFor(sleepSeconds(hi.hour));
}

void loop() {}
