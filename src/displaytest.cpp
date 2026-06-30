// Task 11 Bring-up: 2x2-Layout mit Demo-Daten rendern.
// Nur im [env:displaytest] gebaut.
#include <Arduino.h>
#include <cstring>
#include "display_view.h"
#include "config.h"

static SensorReading mk(const char* id, float t, int h, int b, bool v) {
  SensorReading r{}; strncpy(r.id, id, sizeof(r.id) - 1);
  r.temperature = t; r.humidity = h; r.battery = b; r.valid = v; return r;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("display test start");
  displayInit();
  SensorReading demo[4] = {
    mk("a", 21.4f, 65, 88, true),
    mk("b",  4.9f, 72, 91, true),   // kalt -> blau
    mk("c", 27.6f, 48, 15, true),   // warm -> rot, Batt<20 -> rot
    mk("d",  0,     0, -1, false),  // keine Daten
  };
  HeaderInfo h{22.8f, 50, 84, true, 14, 30, true};
  displayRender(demo, 4, h);
  Serial.println("display test done");
}

void loop() {}
