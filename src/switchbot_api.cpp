#include "switchbot_api.h"
#include <ArduinoJson.h>
bool parseStatusJson(const char* json, SensorReading& out){
  JsonDocument doc;
  if(deserializeJson(doc,json)) { out.valid=false; return false; }
  if(doc["statusCode"].as<int>()!=100){ out.valid=false; return false; }
  JsonObject b=doc["body"];
  if(b["temperature"].isNull()){ out.valid=false; return false; }
  out.temperature=b["temperature"].as<float>();
  out.humidity=b["humidity"].as<int>();
  out.battery=b["battery"].is<int>()?b["battery"].as<int>():-1;
  out.valid=true; return true;
}

#if !defined(ARDUINO)
int fetchAll(const char* const*, SensorReading*, int){ return 0; }
#else
#include <Arduino.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "sb_sign.h"
#include "secrets.h"

static String sbNonce() {
  char b[17];
  for (int i = 0; i < 16; i++) b[i] = "0123456789abcdef"[esp_random() & 15];
  b[16] = 0;
  return String(b);
}

int fetchAll(const char* const* ids, SensorReading* readings, int n) {
  int ok = 0;
  // Alle Geraete liegen hinter demselben Host: eine TLS-Verbindung offen halten
  // spart pro weiterem Geraet einen kompletten Handshake (~1 s Wachzeit).
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  http.setReuse(true);
  for (int i = 0; i < n; i++) {
    strncpy(readings[i].id, ids[i], sizeof(readings[i].id) - 1);
    readings[i].id[sizeof(readings[i].id) - 1] = 0;
    readings[i].valid = false;
    String t  = String((uint64_t)time(nullptr) * 1000ULL);
    String nc = sbNonce();
    String sign = String(sbSign(SB_TOKEN, SB_SECRET, t.c_str(), nc.c_str()).c_str());
    String url = String("https://api.switch-bot.com/v1.1/devices/") + ids[i] + "/status";
    if (!http.begin(cli, url)) continue;
    http.addHeader("Authorization", SB_TOKEN);
    http.addHeader("sign", sign);
    http.addHeader("t", t);
    http.addHeader("nonce", nc);
    http.addHeader("Content-Type", "application/json; charset=utf8");
    int code = http.GET();
    if (code == 200) {
      String body = http.getString();
      if (parseStatusJson(body.c_str(), readings[i])) ok++;
    } else {
      Serial.printf("SwitchBot %s HTTP %d\n", ids[i], code);
    }
    http.end();
  }
  cli.stop();   // Verbindung erst nach dem letzten Geraet wirklich schliessen
  return ok;
}
#endif
