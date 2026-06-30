// Isolierter Bring-up: I2C-Scan + Serial-Hello (Task 1 Smoke + Task 2 Scan).
// Wird nur im [env:scan] gebaut (build_src_filter). Kein WiFi/Display.
#include <Arduino.h>
#include <Wire.h>

// I2C-Kandidaten laut Repo (Codec-Bus): SDA=47, SCL=48.
#define SDA_PIN 47
#define SCL_PIN 48

static void scan() {
  Serial.println("I2C scan...");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found 0x%02X\n", a);
      found++;
    }
  }
  Serial.printf("done (%d Geraete)\n", found);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("PhotoPainter bring-up: boot OK");
  Wire.begin(SDA_PIN, SCL_PIN);
  scan();
}

void loop() {
  delay(5000);
  scan();
}
