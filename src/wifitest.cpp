// WLAN-Diagnose (ohne Scan): Verbindungsversuch mit Statuscode. Schlaeft NICHT.
#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

static const char* stName(wl_status_t st) {
  switch (st) {
    case WL_CONNECTED:     return "CONNECTED";
    case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL (SSID nicht gefunden / 5GHz?)";
    case WL_CONNECT_FAILED:return "CONNECT_FAILED (Passwort falsch?)";
    case WL_IDLE_STATUS:   return "IDLE";
    case WL_DISCONNECTED:  return "DISCONNECTED/connecting...";
    default:               return "?";
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== WiFi-Diagnose (ohne Scan) ===");
  Serial.printf("SSID='%s'  PassLen=%d\n", WIFI_SSID, (int)strlen(WIFI_PASS));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  static int sec = 0;
  wl_status_t st = WiFi.status();
  Serial.printf("t=%2ds status=%d %s\n", sec, st, stName(st));
  if (st == WL_CONNECTED) {
    Serial.printf("  >> IP=%s RSSI=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    delay(5000);
  }
  sec++;
  delay(1000);
}
