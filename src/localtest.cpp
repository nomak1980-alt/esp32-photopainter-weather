// Task 10 Bring-up: lokale Sensoren auslesen (SHTC3 / AXP2101 / PCF85063).
// Nur im [env:localtest] gebaut.
#include <Arduino.h>
#include "local_sensors.h"

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("local sensors test");
  localInit();
  delay(100);
}

void loop() {
  float t = 0; int h = 0, p = 0; bool c = false; struct tm n{};
  bool s_ok = readSHTC3(t, h);
  bool b_ok = readBattery(p, c);
  bool r_ok = rtcNow(n);
  Serial.printf("SHTC3(%d): %.1f C  %d %%  | Batt(%d): %d %% chg=%d | RTC(%d): %04d-%02d-%02d %02d:%02d:%02d\n",
                s_ok, t, h, b_ok, p, c, r_ok,
                n.tm_year + 1900, n.tm_mon + 1, n.tm_mday, n.tm_hour, n.tm_min, n.tm_sec);
  delay(3000);
}
