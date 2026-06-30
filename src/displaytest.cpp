// Task 11 Bring-up: 2x2-Layout mit Demo-Daten rendern.
// Nur im [env:displaytest] gebaut.
#include <Arduino.h>
#include <cstring>
#include "display_view.h"
#include "local_sensors.h"
#include "config.h"

static SensorReading mk(const char* id, float t, int h, int b, bool v) {
  SensorReading r{}; strncpy(r.id, id, sizeof(r.id) - 1);
  r.temperature = t; r.humidity = h; r.battery = b; r.valid = v; return r;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("display test start");
  localInit();        // aktiviert ALDO-Schienen (E-Paper-Strom)
  displayInit();
  SensorReading demo[4] = {
    mk("a", 21.4f, 65, 88, true),
    mk("b",  4.9f, 72, 91, true),   // kalt -> blau
    mk("c", 27.6f, 48, 15, true),   // warm -> rot, Batt<20 -> rot
    mk("d",  0,     0, -1, false),  // keine Daten
  };
  HeaderInfo h{22.8f, 50, 84, true, 14, 30, true};
  // Demo-Vorhersage 7 Tage (Codes: 0 klar,2 heiter,3 bewoelkt,61 Regen,95 Gewitter,75 Schnee)
  int codes[7] = {0, 2, 3, 61, 95, 75, 1};
  int tmax[7]  = {34, 30, 27, 26, 27, 26, 29};
  int tmin[7]  = {21, 17, 18, 14, 14, 17, 18};
  DayForecast fc[7];
  for (int i = 0; i < 7; i++) {
    fc[i].wmoCode = codes[i]; fc[i].tMax = tmax[i]; fc[i].tMin = tmin[i];
    fc[i].wday = (2 + i) % 7; fc[i].valid = true;   // ab Dienstag
  }
  displayRender(demo, 4, h, fc, 7);
  Serial.println("display test done");
}

void loop() {}
