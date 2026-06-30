# ESP32-S3-PhotoPainter „SwitchBot Wetter" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Auf dem Waveshare ESP32-S3-PhotoPainter (7,3" E6-Farb-E-Paper) die SwitchBot-Wetterdaten (4 Sensoren über die SwitchBot Cloud / Hub 3) plus den lokalen SHTC3-Wert anzeigen, batteriebetrieben mit Deep-Sleep.

**Architecture:** Arduino-Firmware (PlatformIO). Bei jedem Wakeup: lokale Sensoren (SHTC3, PMIC-Akku, PCF85063-RTC) lesen → WLAN → SwitchBot Cloud API (HMAC-SHA256-signiert) → Werte mit RTC-RAM vergleichen → nur bei Änderung Vollbild-Refresh des E-Papers via GxEPD2 → Deep-Sleep. Reine Logik (Parsing, Signatur, Intervall, Layout-Formatierung) ist hardwareunabhängig und wird mit PlatformIO-`native`-Host-Unit-Tests per TDD entwickelt; Hardware-Glue wird auf dem Gerät in Bring-up-Tasks verifiziert.

**Tech Stack:** PlatformIO, Arduino-ESP32 (espressif32 Platform), ESP32-S3-WROOM-1-N16R8. Libraries: `GxEPD2` (E-Paper), `Adafruit GFX`, `ArduinoJson` (v7), `XPowersLib` (PMIC, vorbehaltlich I²C-Scan), eine SHTC3-Lib (Adafruit SHTC3) und eine PCF85063-RTC-Lib. HMAC-SHA256/Base64 via mbedTLS (in ESP32-Core enthalten). Host-Tests mit Unity (PlatformIO `native`).

## Global Constraints

- **Board:** ESP32-S3-WROOM-1-N16R8 — 16 MB Flash, 8 MB PSRAM. PlatformIO env `board = esp32-s3-devkitc-1`, `board_upload.flash_size = 16MB`, `board_build.arduino.memory_type = qio_opi` (PSRAM), `build_flags = -DBOARD_HAS_PSRAM`.
- **Upload/Monitor:** nativer USB (VID 303A), aktuell `COM3`. `monitor_speed = 115200`. Für native USB-CDC: `build_flags` zusätzlich `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`.
- **Display:** 7,3" Spectra-6 / E6, 800×480, 6 Farben. GxEPD2-Klasse `GxEPD2_730c_GDEP073E01`. **Nur Vollbild-Refresh** (kein Partial). Farbkonstanten: `GxEPD_BLACK, GxEPD_WHITE, GxEPD_RED, GxEPD_YELLOW, GxEPD_BLUE, GxEPD_GREEN`.
- **Geheimnisse:** WLAN-Passwort, SwitchBot-Token + Secret stehen NUR in `include/secrets.h`. Diese Datei ist in `.gitignore`; eingecheckt wird nur `include/secrets.example.h`.
- **Sensoren-Mapping (deviceId = MAC ohne Doppelpunkte, Großbuchstaben):**
  - „Außen Hinten" = `AAAAAAAAAAAA`
  - „Außen Vorne" = `BBBBBBBBBBBB`
  - „Büro" = `CCCCCCCCCCCC`
  - „Küche" = `DDDDDDDDDDDD`
- **SwitchBot API:** Base-URL `https://api.switch-bot.com`, Status-Endpoint `/v1.1/devices/{deviceId}/status`. Header: `Authorization: <token>`, `sign: <Base64(HMAC_SHA256(secret, token + t + nonce))>` (sign als UPPERCASE-Hex? NEIN — Base64), `t: <ms-Epoch>`, `nonce: <uuid/zufall>`, `Content-Type: application/json; charset=utf8`.
- **Temp-Farblogik:** `< 10 °C` blau, `10–25 °C` grün, `> 25 °C` rot. **Sensor-Batterie < 20 %** → rote Zahl.
- **Wakeup:** Tag (05:00–23:59) alle 10 min, Nacht (00:00–04:59) alle 30 min. Refresh nur bei Änderung von Temp/Feuchte/Batterie eines beliebigen Werts.

---

## File Structure

```
platformio.ini
include/
  secrets.example.h        # Vorlage (eingecheckt)
  secrets.h                # echte Keys (gitignored)
  config.h                 # Sensor-Mapping, Intervalle, Pins
src/
  main.cpp                 # Orchestrierung + Deep-Sleep
  switchbot_api.h/.cpp     # HTTPS-Call + JSON-Parsing
  sb_sign.h/.cpp           # HMAC-SHA256-Signatur (hostfähig)
  reading.h                # SensorReading struct + Vergleich
  power_logic.h/.cpp       # Intervall- & Änderungslogik (hostfähig)
  view_model.h/.cpp        # Werte → Formatierung/Farben (hostfähig)
  display_view.h/.cpp      # GxEPD2-Zeichnen (Hardware)
  local_sensors.h/.cpp     # SHTC3 + PMIC + PCF85063 (Hardware)
lib/                       # ggf. vendored libs
test/
  test_sb_sign/            # native
  test_switchbot_parse/    # native
  test_power_logic/        # native
  test_view_model/         # native
docs/superpowers/...
reference/                 # geklontes Waveshare-Repo (gitignored)
```

**Trennung hostfähig vs. Hardware:** `sb_sign`, `switchbot_api`-Parsing, `power_logic`, `view_model`, `reading` enthalten **keine** Arduino-/Hardware-Includes in ihren testbaren Funktionen, damit `native`-Tests sie ohne Gerät bauen. `display_view` und `local_sensors` sind reine Hardware-Adapter.

---

## Task 1: PlatformIO-Projekt + Build + Serial-Hello auf COM3

**Files:**
- Create: `platformio.ini`
- Create: `src/main.cpp`
- Create: `.gitignore`

**Interfaces:**
- Produces: lauffähiges Grundgerüst, `setup()/loop()` mit Serial-Ausgabe.

- [ ] **Step 1: `.gitignore` anlegen**

```
.pio/
reference/
include/secrets.h
```

- [ ] **Step 2: `platformio.ini` anlegen**

```ini
[platformio]
default_envs = photopainter

[env:photopainter]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
upload_port = COM3
monitor_port = COM3
board_upload.flash_size = 16MB
board_build.arduino.memory_type = qio_opi
build_flags =
  -DBOARD_HAS_PSRAM
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1

[env:native]
platform = native
test_framework = unity
build_flags = -std=gnu++17
```

- [ ] **Step 3: minimal `src/main.cpp`**

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("PhotoPainter Wetter: boot OK");
}

void loop() {
  Serial.println("alive");
  delay(5000);
}
```

- [ ] **Step 4: Bauen**

Run: `pio run -e photopainter`
Expected: `SUCCESS`

- [ ] **Step 5: Flashen + Monitor**

Run: `pio run -e photopainter -t upload && pio device monitor -e photopainter`
Expected: Serial zeigt `PhotoPainter Wetter: boot OK` und periodisch `alive`. (BOOT halten + Reset, falls Upload nicht startet.)

- [ ] **Step 6: Commit**

```bash
git init
git add .gitignore platformio.ini src/main.cpp
git commit -m "chore: PlatformIO-Grundgeruest, Serial-Hello laeuft auf COM3"
```

---

## Task 2: Hardware-Bring-up — I²C-Scan (Adressen ermitteln)

**Ziel:** Tatsächliche I²C-Adressen + aktiven Bus (Pins) der Onboard-Chips bestimmen — klärt insbesondere PMIC (TG28 vs AXP2101).

**Files:**
- Create: `tools/i2c_scan/i2c_scan.ino` (separater Sketch, nur Bring-up)
- Create: `reference/` (Klon des Waveshare-Repos zur Pin-Recherche)

**Interfaces:**
- Produces: dokumentierte I²C-Adressen + SDA/SCL-Pins in `include/config.h` (in Task 3 angelegt/erweitert). Erwartung laut Datenblättern: SHTC3 `0x70`, PCF85063 `0x51`, AXP2101 `0x34` (falls verbaut).

- [ ] **Step 1: Referenz-Repo klonen**

```bash
git clone --depth 1 https://github.com/waveshareteam/ESP32-S3-PhotoPainter reference/photopainter
```

- [ ] **Step 2: SDA/SCL-Kandidaten aus Repo bestätigen**

Run: `grep -rniE "i2c|sda|scl|GPIO_NUM" reference/photopainter/01_Example/xiaozhi-esp32/main/boards/waveshare-s3-PhotoPainter/ reference/photopainter/05_ArduinoExample/01_Audio_Test/`
Notiere die SDA/SCL-Pins (Codec-Bus ist SDA=47/SCL=48; prüfen, ob Sensoren am selben Bus hängen).

- [ ] **Step 3: I²C-Scan-Sketch schreiben** (probiert die wahrscheinlichen Buspins)

```cpp
#include <Arduino.h>
#include <Wire.h>
// Kandidaten aus Step 2 — falls abweichend, hier anpassen:
#define SDA_PIN 47
#define SCL_PIN 48
void setup() {
  Serial.begin(115200); delay(1500);
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("I2C scan...");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf("  found 0x%02X\n", a);
  }
  Serial.println("done");
}
void loop() {}
```

- [ ] **Step 4: Scan flashen und Adressen ablesen**

Run: temporär `src/main.cpp` durch den Scan-Inhalt ersetzen, `pio run -e photopainter -t upload && pio device monitor`
Expected: Liste gefundener Adressen. **Erwartung:** `0x70` (SHTC3), `0x51` (PCF85063), PMIC (`0x34` = AXP2101 ⇒ XPowersLib nutzbar). Falls KEIN `0x34`: PMIC ist nicht AXP2101 → in Task 7 alternative Auslesung (TG28-Datenblatt / Repo-Code) wählen.

- [ ] **Step 5: Befund dokumentieren**

Schreibe die bestätigten Adressen + Buspins als Kommentar oben in eine neue Datei `include/config.h`:

```cpp
#pragma once
// --- I2C (bestaetigt per Scan in Task 2) ---
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define SHTC3_I2C_ADDR   0x70
#define PCF85063_I2C_ADDR 0x51
#define PMIC_I2C_ADDR    0x34   // 0x34 = AXP2101; anpassen falls Scan abweicht
```

- [ ] **Step 6: `main.cpp` zuruecksetzen + Commit**

`src/main.cpp` auf den Serial-Hello-Stand aus Task 1 zurücksetzen.

```bash
git add include/config.h src/main.cpp
git commit -m "chore: I2C-Scan, Adressen bestaetigt und in config.h dokumentiert"
```

---

## Task 3: Bring-up E-Paper — GxEPD2-Testbild

**Files:**
- Modify: `platformio.ini` (lib_deps)
- Modify: `include/config.h` (EPD-Pins)
- Create: `tools/epd_test/epd_test.ino` (temporär in main.cpp gespiegelt)

**Interfaces:**
- Produces: bestätigte EPD-SPI-Pins in `config.h`; Nachweis, dass GxEPD2 das Panel ansteuert.

- [ ] **Step 1: EPD-Pins aus Repo/Schaltplan ermitteln**

Run: `grep -rniE "busy|reset|rst|dc|cs|sck|clk|mosi|sdi|epd|spi" reference/photopainter/01_Example/xiaozhi-esp32/components/`
Trage die gefundenen Pins (BUSY, RST, DC, CS, SCK, MOSI) in `include/config.h` ein:

```cpp
// --- E-Paper SPI (bestaetigt aus Repo/Schaltplan in Task 3) ---
#define EPD_CS_PIN   <pin>
#define EPD_DC_PIN   <pin>
#define EPD_RST_PIN  <pin>
#define EPD_BUSY_PIN <pin>
#define EPD_SCK_PIN  <pin>
#define EPD_MOSI_PIN <pin>
```
(Werte sind hier bewusst aus der Hardware zu lesen — kein Rateversuch im Plan.)

- [ ] **Step 2: GxEPD2 als Dependency**

In `platformio.ini` unter `[env:photopainter]`:

```ini
lib_deps =
  zinggjm/GxEPD2@^1.6.0
  adafruit/Adafruit GFX Library@^1.11.9
```

- [ ] **Step 3: Testbild-Sketch (Farbbalken + Text)**

```cpp
#include <Arduino.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "config.h"

GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT> display(
  GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

void setup() {
  Serial.begin(115200); delay(1000);
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    uint16_t cols[6] = {GxEPD_BLACK,GxEPD_RED,GxEPD_YELLOW,GxEPD_BLUE,GxEPD_GREEN,GxEPD_WHITE};
    for (int i=0;i<6;i++) display.fillRect(0, i*80, 800, 80, cols[i]);
    display.setFont(&FreeSansBold24pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(40, 120);
    display.print("PhotoPainter OK");
  } while (display.nextPage());
  Serial.println("EPD draw done");
}
void loop() {}
```

- [ ] **Step 4: Flashen und Display prüfen**

Run: main.cpp temporär durch Sketch ersetzen, `pio run -e photopainter -t upload && pio device monitor`
Expected: Display zeigt nach ~20 s Farbbalken + „PhotoPainter OK"; Serial: `EPD draw done`. Falls Klasse/Panel abweicht: in GxEPD2-Beispielen die passende 7.3"-E6-Klasse wählen (`GxEPD2_730c_*`).

- [ ] **Step 5: Commit**

```bash
git add platformio.ini include/config.h src/main.cpp
git commit -m "feat: E-Paper Bring-up mit GxEPD2, Testbild laeuft"
```

---

## Task 4: `reading.h` — Datenmodell + Gleichheit (native)

**Files:**
- Create: `src/reading.h`
- Create: `test/test_power_logic/test_reading.cpp` (mit Task 6 zusammen ausgeführt)

**Interfaces:**
- Produces:
  ```cpp
  struct SensorReading {
    char  id[16];        // deviceId, "" = Slot leer
    float temperature;   // °C
    int   humidity;      // %
    int   battery;       // %, -1 = unbekannt
    bool  valid;         // Daten vorhanden
  };
  bool sameValues(const SensorReading& a, const SensorReading& b); // temp(1 Dezimal)+hum+batt gleich
  ```

- [ ] **Step 1: Test schreiben**

`test/test_power_logic/test_reading.cpp`:

```cpp
#include <unity.h>
#include "reading.h"
void test_same_values_true_when_equal() {
  SensorReading a{"x",21.4f,65,88,true};
  SensorReading b{"x",21.44f,65,88,true}; // gerundet gleich
  TEST_ASSERT_TRUE(sameValues(a,b));
}
void test_same_values_false_on_temp_change() {
  SensorReading a{"x",21.4f,65,88,true};
  SensorReading b{"x",21.6f,65,88,true};
  TEST_ASSERT_FALSE(sameValues(a,b));
}
void test_same_values_false_on_validity_change() {
  SensorReading a{"x",0,0,-1,false};
  SensorReading b{"x",0,0,-1,true};
  TEST_ASSERT_FALSE(sameValues(a,b));
}
// runner kommt in Task 6 (gemeinsamer main); hier nur Testfunktionen.
```

- [ ] **Step 2: `reading.h` implementieren**

```cpp
#pragma once
#include <cmath>
#include <cstring>
struct SensorReading {
  char  id[16];
  float temperature;
  int   humidity;
  int   battery;
  bool  valid;
};
inline int tenths(float t){ return (int)lround(t*10.0); }
inline bool sameValues(const SensorReading& a, const SensorReading& b){
  if (a.valid != b.valid) return false;
  if (!a.valid) return true;
  return tenths(a.temperature)==tenths(b.temperature)
      && a.humidity==b.humidity && a.battery==b.battery;
}
```

- [ ] **Step 3: Build/Test erfolgt in Task 6** (gemeinsamer native-Runner). Hier nur Commit.

```bash
git add src/reading.h test/test_power_logic/test_reading.cpp
git commit -m "feat: SensorReading-Modell + Wertvergleich"
```

---

## Task 5: `sb_sign` — HMAC-SHA256-Signatur (native)

**Files:**
- Create: `src/sb_sign.h`, `src/sb_sign.cpp`
- Create: `test/test_sb_sign/test_sb_sign.cpp`
- Create: `test/test_sb_sign/main.cpp` (Unity-Runner)

**Interfaces:**
- Produces:
  ```cpp
  // Base64(HMAC_SHA256(secret, token + t + nonce))
  std::string sbSign(const std::string& token, const std::string& secret,
                     const std::string& t, const std::string& nonce);
  ```
- Consumes: nichts. Implementierung nutzt **mbedTLS** auf dem Gerät; im native-Test wird dieselbe Quelle gegen einen bekannten Vektor geprüft (mbedTLS ist auch nativ verfügbar; falls nicht installiert, env `native` mit `lib_deps`/System-mbedTLS, sonst Test mit eingebetteter Mini-SHA256-Referenz). Wir verwenden mbedTLS und linken systemweit.

- [ ] **Step 1: Bekannten Vektor festlegen (Test)**

`test/test_sb_sign/test_sb_sign.cpp`:

```cpp
#include <unity.h>
#include "sb_sign.h"
// Referenz extern berechnet (z.B. python hmac) fuer feste Eingaben:
// token="TKN", secret="SEC", t="1700000000000", nonce="abc"
void test_sign_known_vector() {
  std::string s = sbSign("TKN","SEC","1700000000000","abc");
  TEST_ASSERT_EQUAL_STRING("Q2k3l...REPLACE_WITH_REAL_BASE64...", s.c_str());
}
```
> Vor dem Implementieren den echten Referenzwert erzeugen:
> `python -c "import hmac,hashlib,base64;print(base64.b64encode(hmac.new(b'SEC',b'TKN1700000000000abc',hashlib.sha256).digest()).decode())"`
> und in den Test eintragen.

- [ ] **Step 2: Test ausführen (rot)**

Run: `pio test -e native -f test_sb_sign`
Expected: FAIL (sbSign undefiniert).

- [ ] **Step 3: Implementieren**

`src/sb_sign.cpp`:

```cpp
#include "sb_sign.h"
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
std::string sbSign(const std::string& token, const std::string& secret,
                   const std::string& t, const std::string& nonce){
  std::string msg = token + t + nonce;
  unsigned char mac[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(info,(const unsigned char*)secret.data(),secret.size(),
                  (const unsigned char*)msg.data(),msg.size(),mac);
  unsigned char b64[64]; size_t olen=0;
  mbedtls_base64_encode(b64,sizeof(b64),&olen,mac,32);
  return std::string((char*)b64,olen);
}
```
`src/sb_sign.h`:
```cpp
#pragma once
#include <string>
std::string sbSign(const std::string&,const std::string&,const std::string&,const std::string&);
```

- [ ] **Step 4: Unity-Runner**

`test/test_sb_sign/main.cpp`:
```cpp
#include <unity.h>
void test_sign_known_vector();
int main(){ UNITY_BEGIN(); RUN_TEST(test_sign_known_vector); return UNITY_END(); }
```

- [ ] **Step 5: Test grün**

Run: `pio test -e native -f test_sb_sign`
Expected: PASS. (Falls mbedTLS nativ fehlt: env `native` um Systemlib ergänzen, z. B. `build_flags = -lmbedcrypto`.)

- [ ] **Step 6: Commit**

```bash
git add src/sb_sign.* test/test_sb_sign/
git commit -m "feat: SwitchBot HMAC-SHA256-Signatur mit Host-Test"
```

---

## Task 6: `power_logic` — Intervall + Änderungserkennung (native)

**Files:**
- Create: `src/power_logic.h`, `src/power_logic.cpp`
- Create: `test/test_power_logic/test_power_logic.cpp`, `test/test_power_logic/main.cpp`

**Interfaces:**
- Consumes: `SensorReading`, `sameValues` (Task 4).
- Produces:
  ```cpp
  uint32_t sleepSeconds(int hour);                 // 5..23 ->600, 0..4 ->1800
  bool anyChanged(const SensorReading* now, const SensorReading* prev, int n);
  ```

- [ ] **Step 1: Tests schreiben**

`test/test_power_logic/test_power_logic.cpp`:
```cpp
#include <unity.h>
#include "power_logic.h"
void test_day_interval(){ TEST_ASSERT_EQUAL_UINT32(600, sleepSeconds(14)); }
void test_night_interval(){ TEST_ASSERT_EQUAL_UINT32(1800, sleepSeconds(2)); }
void test_boundaries(){ TEST_ASSERT_EQUAL_UINT32(1800,sleepSeconds(0));
                        TEST_ASSERT_EQUAL_UINT32(600,sleepSeconds(5)); }
void test_any_changed(){
  SensorReading now[1]={{"x",21.4f,65,88,true}};
  SensorReading prev[1]={{"x",21.4f,66,88,true}};
  TEST_ASSERT_TRUE(anyChanged(now,prev,1));
  prev[0].humidity=65;
  TEST_ASSERT_FALSE(anyChanged(now,prev,1));
}
```

- [ ] **Step 2: gemeinsamer Runner** `test/test_power_logic/main.cpp`:
```cpp
#include <unity.h>
void test_day_interval(); void test_night_interval(); void test_boundaries();
void test_any_changed();
void test_same_values_true_when_equal(); void test_same_values_false_on_temp_change();
void test_same_values_false_on_validity_change();
int main(){ UNITY_BEGIN();
  RUN_TEST(test_same_values_true_when_equal);
  RUN_TEST(test_same_values_false_on_temp_change);
  RUN_TEST(test_same_values_false_on_validity_change);
  RUN_TEST(test_day_interval); RUN_TEST(test_night_interval);
  RUN_TEST(test_boundaries); RUN_TEST(test_any_changed);
  return UNITY_END(); }
```

- [ ] **Step 3: Test rot**

Run: `pio test -e native -f test_power_logic`
Expected: FAIL.

- [ ] **Step 4: Implementieren** `src/power_logic.cpp`:
```cpp
#include "power_logic.h"
uint32_t sleepSeconds(int hour){ return (hour>=0 && hour<5) ? 1800u : 600u; }
bool anyChanged(const SensorReading* now,const SensorReading* prev,int n){
  for(int i=0;i<n;i++) if(!sameValues(now[i],prev[i])) return true;
  return false;
}
```
`src/power_logic.h`:
```cpp
#pragma once
#include <cstdint>
#include "reading.h"
uint32_t sleepSeconds(int hour);
bool anyChanged(const SensorReading* now,const SensorReading* prev,int n);
```

- [ ] **Step 5: Test grün**

Run: `pio test -e native -f test_power_logic`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/power_logic.* src/reading.h test/test_power_logic/
git commit -m "feat: Intervall- und Aenderungslogik mit Host-Tests"
```

---

## Task 7: `view_model` — Formatierung + Farben (native)

**Files:**
- Create: `src/view_model.h`, `src/view_model.cpp`
- Create: `test/test_view_model/test_view_model.cpp`, `test/test_view_model/main.cpp`

**Interfaces:**
- Consumes: `SensorReading`.
- Produces:
  ```cpp
  enum Col { COL_BLACK, COL_RED, COL_YELLOW, COL_BLUE, COL_GREEN, COL_WHITE };
  Col tempColor(float c);          // <10 blau,10-25 gruen,>25 rot
  bool batteryWarn(int battery);   // 0..19 -> true; -1 -> false
  // formatiert "21.4" / "65" / "Batt 88%" / "-- --"
  void fmtTemp(const SensorReading&, char* out, size_t n);
  void fmtHum (const SensorReading&, char* out, size_t n);
  void fmtBatt(const SensorReading&, char* out, size_t n);
  ```

- [ ] **Step 1: Tests**

`test/test_view_model/test_view_model.cpp`:
```cpp
#include <unity.h>
#include <cstring>
#include "view_model.h"
void test_temp_color(){
  TEST_ASSERT_EQUAL(COL_BLUE, tempColor(5));
  TEST_ASSERT_EQUAL(COL_GREEN,tempColor(20));
  TEST_ASSERT_EQUAL(COL_RED,  tempColor(30));
  TEST_ASSERT_EQUAL(COL_GREEN,tempColor(10));   // Grenze
  TEST_ASSERT_EQUAL(COL_GREEN,tempColor(25));   // Grenze
}
void test_batt_warn(){
  TEST_ASSERT_TRUE(batteryWarn(15));
  TEST_ASSERT_FALSE(batteryWarn(20));
  TEST_ASSERT_FALSE(batteryWarn(-1));
}
void test_fmt(){
  char b[16]; SensorReading r{"x",21.4f,65,88,true};
  fmtTemp(r,b,sizeof b); TEST_ASSERT_EQUAL_STRING("21.4",b);
  fmtHum(r,b,sizeof b);  TEST_ASSERT_EQUAL_STRING("65",b);
  fmtBatt(r,b,sizeof b); TEST_ASSERT_EQUAL_STRING("Batt 88%",b);
  SensorReading e{"x",0,0,-1,false};
  fmtTemp(e,b,sizeof b); TEST_ASSERT_EQUAL_STRING("-- --",b);
}
```
`test/test_view_model/main.cpp`:
```cpp
#include <unity.h>
void test_temp_color(); void test_batt_warn(); void test_fmt();
int main(){ UNITY_BEGIN(); RUN_TEST(test_temp_color); RUN_TEST(test_batt_warn);
  RUN_TEST(test_fmt); return UNITY_END(); }
```

- [ ] **Step 2: Test rot**

Run: `pio test -e native -f test_view_model`
Expected: FAIL.

- [ ] **Step 3: Implementieren** `src/view_model.cpp`:
```cpp
#include "view_model.h"
#include <cstdio>
Col tempColor(float c){ if(c<10) return COL_BLUE; if(c>25) return COL_RED; return COL_GREEN; }
bool batteryWarn(int b){ return b>=0 && b<20; }
void fmtTemp(const SensorReading& r,char* o,size_t n){
  if(!r.valid){ snprintf(o,n,"-- --"); return; } snprintf(o,n,"%.1f",r.temperature); }
void fmtHum(const SensorReading& r,char* o,size_t n){
  if(!r.valid){ snprintf(o,n,"--"); return; } snprintf(o,n,"%d",r.humidity); }
void fmtBatt(const SensorReading& r,char* o,size_t n){
  if(!r.valid||r.battery<0){ snprintf(o,n," "); return; } snprintf(o,n,"Batt %d%%",r.battery); }
```
`src/view_model.h`:
```cpp
#pragma once
#include <cstddef>
#include "reading.h"
enum Col { COL_BLACK, COL_RED, COL_YELLOW, COL_BLUE, COL_GREEN, COL_WHITE };
Col tempColor(float c);
bool batteryWarn(int battery);
void fmtTemp(const SensorReading&,char*,size_t);
void fmtHum (const SensorReading&,char*,size_t);
void fmtBatt(const SensorReading&,char*,size_t);
```

- [ ] **Step 4: Test grün**

Run: `pio test -e native -f test_view_model`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/view_model.* test/test_view_model/
git commit -m "feat: View-Model Formatierung + Farblogik mit Host-Tests"
```

---

## Task 8: `switchbot_api` — JSON-Parsing (native) + HTTPS-Call (Gerät)

**Files:**
- Create: `src/switchbot_api.h`, `src/switchbot_api.cpp`
- Create: `test/test_switchbot_parse/test_parse.cpp`, `.../main.cpp`
- Modify: `platformio.ini` (lib_deps: ArduinoJson)

**Interfaces:**
- Consumes: `SensorReading` (Task 4), `sbSign` (Task 5).
- Produces:
  ```cpp
  // Parst SwitchBot /status-Body in ein bestehendes Reading (id bleibt erhalten).
  bool parseStatusJson(const char* json, SensorReading& out);
  // Gerät: fuellt readings[] fuer alle deviceIds; gibt Anzahl erfolgreicher zurueck.
  int fetchAll(const char* const* ids, SensorReading* readings, int n);
  ```

- [ ] **Step 1: ArduinoJson hinzufügen** in `platformio.ini` lib_deps:
```ini
  bblanchon/ArduinoJson@^7.0.0
```

- [ ] **Step 2: Parse-Test mit echtem Beispiel-Body**

`test/test_switchbot_parse/test_parse.cpp`:
```cpp
#include <unity.h>
#include "switchbot_api.h"
const char* BODY = R"({"statusCode":100,"body":{"deviceId":"X",
"temperature":21.4,"humidity":65,"battery":88},"message":"success"})";
void test_parse_ok(){
  SensorReading r{"X",0,0,-1,false};
  TEST_ASSERT_TRUE(parseStatusJson(BODY,r));
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_INT(65,r.humidity);
  TEST_ASSERT_EQUAL_INT(88,r.battery);
  TEST_ASSERT_FLOAT_WITHIN(0.01,21.4,r.temperature);
}
void test_parse_error_body(){
  SensorReading r{"X",0,0,-1,false};
  TEST_ASSERT_FALSE(parseStatusJson(R"({"statusCode":190,"body":{}})",r));
  TEST_ASSERT_FALSE(r.valid);
}
```
`test/test_switchbot_parse/main.cpp`:
```cpp
#include <unity.h>
void test_parse_ok(); void test_parse_error_body();
int main(){ UNITY_BEGIN(); RUN_TEST(test_parse_ok); RUN_TEST(test_parse_error_body);
  return UNITY_END(); }
```

- [ ] **Step 3: Test rot**

Run: `pio test -e native -f test_switchbot_parse`
Expected: FAIL.

- [ ] **Step 4: `parseStatusJson` implementieren** (hostfähig, nur ArduinoJson) `src/switchbot_api.cpp` (Teil 1):
```cpp
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
```
`src/switchbot_api.h`:
```cpp
#pragma once
#include "reading.h"
bool parseStatusJson(const char* json, SensorReading& out);
int  fetchAll(const char* const* ids, SensorReading* readings, int n);
```

- [ ] **Step 5: Test grün**

Run: `pio test -e native -f test_switchbot_parse`
Expected: PASS.

- [ ] **Step 6: `fetchAll` implementieren** (Gerät-only, in `#if defined(ARDUINO)`-Guard) `src/switchbot_api.cpp` (Teil 2):
```cpp
#if defined(ARDUINO)
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "sb_sign.h"
#include "secrets.h"   // SB_TOKEN, SB_SECRET
static String nonce(){ char b[17]; for(int i=0;i<16;i++) b[i]="0123456789abcdef"[esp_random()&15]; b[16]=0; return String(b);}
int fetchAll(const char* const* ids, SensorReading* readings, int n){
  int ok=0;
  for(int i=0;i<n;i++){
    strncpy(readings[i].id, ids[i], sizeof(readings[i].id)-1);
    readings[i].valid=false;
    String t=String((uint64_t)time(nullptr)*1000ULL);
    String nc=nonce();
    String sign=String(sbSign(SB_TOKEN,SB_SECRET,t.c_str(),nc.c_str()).c_str());
    WiFiClientSecure cli; cli.setInsecure();
    HTTPClient http;
    String url=String("https://api.switch-bot.com/v1.1/devices/")+ids[i]+"/status";
    if(!http.begin(cli,url)) continue;
    http.addHeader("Authorization", SB_TOKEN);
    http.addHeader("sign", sign);
    http.addHeader("t", t);
    http.addHeader("nonce", nc);
    http.addHeader("Content-Type","application/json; charset=utf8");
    int code=http.GET();
    if(code==200){ String body=http.getString();
      if(parseStatusJson(body.c_str(),readings[i])) ok++; }
    http.end();
  }
  return ok;
}
#endif
```

- [ ] **Step 7: Build fürs Gerät prüfen**

Run: `pio run -e photopainter`
Expected: SUCCESS (benötigt `include/secrets.h` aus Task 9; falls noch nicht da, zuerst Task 9 Step 1-2 vorziehen).

- [ ] **Step 8: Commit**

```bash
git add src/switchbot_api.* platformio.ini test/test_switchbot_parse/
git commit -m "feat: SwitchBot API Parsing (Host-Test) + HTTPS fetchAll"
```

---

## Task 9: Secrets + Config zusammenführen

**Files:**
- Create: `include/secrets.example.h`
- Create: `include/secrets.h` (gitignored)
- Modify: `include/config.h` (Sensor-Mapping, Intervalle)

**Interfaces:**
- Produces: `SB_TOKEN`, `SB_SECRET`, `WIFI_SSID`, `WIFI_PASS`, `DEVICE_IDS[]`, `DEVICE_NAMES[]`, `DEVICE_COUNT`.

- [ ] **Step 1: `include/secrets.example.h`**
```cpp
#pragma once
#define WIFI_SSID  "DEIN_WLAN"
#define WIFI_PASS  "DEIN_PASSWORT"
#define SB_TOKEN   "DEIN_SWITCHBOT_TOKEN"
#define SB_SECRET  "DEIN_SWITCHBOT_SECRET"
```

- [ ] **Step 2: `include/secrets.h` mit echten Werten anlegen** (vom Nutzer; nicht committen).

- [ ] **Step 3: `include/config.h` ergänzen**
```cpp
// --- Sensoren ---
#define DEVICE_COUNT 4
static const char* const DEVICE_IDS[DEVICE_COUNT] =
  {"AAAAAAAAAAAA","BBBBBBBBBBBB","CCCCCCCCCCCC","DDDDDDDDDDDD"};
static const char* const DEVICE_NAMES[DEVICE_COUNT] =
  {"Außen Hinten","Außen Vorne","Büro","Küche"};
static const bool DEVICE_OUTDOOR[DEVICE_COUNT] = {true,true,false,false};
```

- [ ] **Step 4: Commit (ohne secrets.h)**

```bash
git add include/secrets.example.h include/config.h
git commit -m "chore: secrets-Vorlage + Sensor-Mapping in config.h"
```

---

## Task 10: `local_sensors` — SHTC3 + Akku + RTC (Gerät)

**Files:**
- Create: `src/local_sensors.h`, `src/local_sensors.cpp`
- Modify: `platformio.ini` (lib_deps)

**Interfaces:**
- Produces:
  ```cpp
  struct LocalState { float temp; int hum; int battPct; bool charging; struct tm now; bool timeValid; };
  void localInit();
  bool readSHTC3(float& temp, int& hum);
  bool readBattery(int& pct, bool& charging);
  bool rtcNow(struct tm& out);      // PCF85063
  void rtcSet(const struct tm& t);
  ```

- [ ] **Step 1: Libraries** in `platformio.ini` lib_deps:
```ini
  adafruit/Adafruit SHTC3 Library@^1.0.1
  lewisxhe/XPowersLib@^0.2.6
```
(PCF85063: kleine eigene I²C-Routine, kein extra Lib nötig.)

- [ ] **Step 2: Implementieren** `src/local_sensors.cpp`:
```cpp
#include "local_sensors.h"
#include <Wire.h>
#include <Adafruit_SHTC3.h>
#include "config.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
static Adafruit_SHTC3 shtc3;
static XPowersPMU pmu;
void localInit(){ Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN); shtc3.begin(&Wire);
  pmu.begin(Wire, PMIC_I2C_ADDR, I2C_SDA_PIN, I2C_SCL_PIN); }
bool readSHTC3(float& t,int& h){ sensors_event_t hu,te;
  if(!shtc3.getEvent(&hu,&te)) return false; t=te.temperature; h=(int)lround(hu.relative_humidity); return true; }
bool readBattery(int& pct,bool& chg){ pct=pmu.getBatteryPercent(); chg=pmu.isCharging(); return pct>=0; }
// PCF85063 (BCD-Register ab 0x04: sec,min,hour,day,wday,month,year)
static uint8_t bcd2dec(uint8_t b){return (b>>4)*10+(b&0x0F);} 
static uint8_t dec2bcd(uint8_t d){return ((d/10)<<4)|(d%10);} 
bool rtcNow(struct tm& o){ Wire.beginTransmission(PCF85063_I2C_ADDR); Wire.write(0x04);
  if(Wire.endTransmission()!=0) return false;
  if(Wire.requestFrom(PCF85063_I2C_ADDR,7)!=7) return false;
  uint8_t s=Wire.read(),mi=Wire.read(),hh=Wire.read(),dd=Wire.read(),wd=Wire.read(),mo=Wire.read(),yr=Wire.read();
  o.tm_sec=bcd2dec(s&0x7F); o.tm_min=bcd2dec(mi&0x7F); o.tm_hour=bcd2dec(hh&0x3F);
  o.tm_mday=bcd2dec(dd&0x3F); o.tm_mon=bcd2dec(mo&0x1F)-1; o.tm_year=bcd2dec(yr)+100; (void)wd; return true; }
void rtcSet(const struct tm& t){ Wire.beginTransmission(PCF85063_I2C_ADDR); Wire.write(0x04);
  Wire.write(dec2bcd(t.tm_sec)); Wire.write(dec2bcd(t.tm_min)); Wire.write(dec2bcd(t.tm_hour));
  Wire.write(dec2bcd(t.tm_mday)); Wire.write(dec2bcd(0)); Wire.write(dec2bcd(t.tm_mon+1));
  Wire.write(dec2bcd(t.tm_year-100)); Wire.endTransmission(); }
```
`src/local_sensors.h`:
```cpp
#pragma once
#include <time.h>
void localInit();
bool readSHTC3(float& temp,int& hum);
bool readBattery(int& pct,bool& charging);
bool rtcNow(struct tm& out);
void rtcSet(const struct tm& t);
```

- [ ] **Step 3: Bring-up-Print** (temporär in `main.cpp setup()` einfügen):
```cpp
localInit(); float t; int h,p; bool c; struct tm n;
readSHTC3(t,h); readBattery(p,c); rtcNow(n);
Serial.printf("SHTC3 %.1fC %d%% | Batt %d%% chg=%d | RTC %02d:%02d\n",t,h,p,c,n.tm_hour,n.tm_min);
```

- [ ] **Step 4: Flashen + prüfen**

Run: `pio run -e photopainter -t upload && pio device monitor`
Expected: plausible Werte (Raumtemp, Akku %, Uhrzeit). Falls Akku/`getBatteryPercent` unplausibel → PMIC ≠ AXP2101 (Task 2): Auslesung an tatsächlichen Chip anpassen.

- [ ] **Step 5: Commit**

```bash
git add src/local_sensors.* platformio.ini src/main.cpp
git commit -m "feat: lokale Sensoren SHTC3 + Akku + PCF85063-RTC"
```

---

## Task 11: `display_view` — 2×2-Layout + Header zeichnen (Gerät)

**Files:**
- Create: `src/display_view.h`, `src/display_view.cpp`

**Interfaces:**
- Consumes: `SensorReading[]`, `view_model`-Funktionen, `DEVICE_NAMES`, GxEPD2.
- Produces:
  ```cpp
  struct HeaderInfo { float localTemp; int localHum; int battPct; bool charging; int hour; int minute; bool wifiOk; };
  void displayInit();
  void displayRender(const SensorReading* r, int n, const HeaderInfo& h);
  ```

- [ ] **Step 1: Farb-Mapping + Init** `src/display_view.cpp`:
```cpp
#include "display_view.h"
#include <GxEPD2_7C.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include "config.h"
#include "view_model.h"
static GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT> display(
  GxEPD2_730c_GDEP073E01(EPD_CS_PIN,EPD_DC_PIN,EPD_RST_PIN,EPD_BUSY_PIN));
static uint16_t toGx(Col c){ switch(c){case COL_RED:return GxEPD_RED;case COL_BLUE:return GxEPD_BLUE;
  case COL_GREEN:return GxEPD_GREEN;case COL_YELLOW:return GxEPD_YELLOW;case COL_WHITE:return GxEPD_WHITE;
  default:return GxEPD_BLACK;} }
void displayInit(){ SPI.begin(EPD_SCK_PIN,-1,EPD_MOSI_PIN,EPD_CS_PIN); display.init(115200); display.setRotation(0); }
```

- [ ] **Step 2: Render-Funktion** (Header + 4 Kacheln) anhängen:
```cpp
static void drawTile(int x,int y,int w,int h,const char* name,const SensorReading& r){
  display.drawRect(x,y,w,h,GxEPD_BLACK);
  display.setFont(&FreeSansBold12pt7b); display.setTextColor(GxEPD_BLACK);
  display.setCursor(x+12,y+28); display.print(name);
  display.setCursor(x+w-70,y+28);
  display.setTextColor(r.valid?GxEPD_GREEN:GxEPD_BLACK); display.print(r.valid?"ON":"--");
  char buf[16];
  if(r.valid){
    fmtTemp(r,buf,sizeof buf); display.setFont(&FreeSansBold18pt7b);
    display.setTextColor(toGx(tempColor(r.temperature)));
    display.setCursor(x+16,y+74); display.print(buf); display.print(" C");
    fmtHum(r,buf,sizeof buf); display.setFont(&FreeSans9pt7b); display.setTextColor(GxEPD_BLACK);
    display.setCursor(x+16,y+104); display.print(buf); display.print(" %");
    fmtBatt(r,buf,sizeof buf); display.setTextColor(batteryWarn(r.battery)?GxEPD_RED:GxEPD_BLACK);
    display.setCursor(x+16,y+130); display.print(buf);
  } else {
    display.setFont(&FreeSans9pt7b); display.setTextColor(GxEPD_BLACK);
    display.setCursor(x+16,y+80); display.print("-- keine Daten --");
  }
}
void displayRender(const SensorReading* r,int n,const HeaderInfo& hi){
  display.setFullWindow(); display.firstPage();
  do{
    display.fillScreen(GxEPD_WHITE);
    // Header
    display.setFont(&FreeSansBold12pt7b); display.setTextColor(GxEPD_BLACK);
    display.setCursor(12,30); display.print("SwitchBot Wetter");
    char hb[48];
    snprintf(hb,sizeof hb,"Hier: %.1fC %d%%   %02d:%02d   Akku %d%%%s",
      hi.localTemp,hi.localHum,hi.hour,hi.minute,hi.battPct,hi.charging?" +":"");
    display.setFont(&FreeSans9pt7b); display.setCursor(300,26); display.print(hb);
    display.drawLine(0,44,800,44,GxEPD_BLACK);
    // 2x2 (Aussen oben/Innen unten anhand config-Reihenfolge)
    int gx=8, gy=52, gw=(800-24)/2, gh=(480-52-8)/2;
    int pos=0;
    for(int row=0; row<2 && pos<n; row++)
      for(int col=0; col<2 && pos<n; col++,pos++)
        drawTile(gx+col*(gw+8), gy+row*(gh+4), gw, gh, DEVICE_NAMES[pos], r[pos]);
  } while(display.nextPage());
}
```
`src/display_view.h`:
```cpp
#pragma once
#include "reading.h"
struct HeaderInfo { float localTemp; int localHum; int battPct; bool charging; int hour; int minute; bool wifiOk; };
void displayInit();
void displayRender(const SensorReading* r,int n,const HeaderInfo& h);
```

- [ ] **Step 3: Smoke-Test mit Dummy-Daten** (temporär in `main.cpp`):
```cpp
displayInit();
SensorReading demo[4]={{"a",21.4f,65,88,true},{"b",4.9f,72,91,true},
                       {"c",23.1f,48,76,true},{"d",0,0,-1,false}};
HeaderInfo h{22.8f,50,84,true,14,30,true};
displayRender(demo,4,h);
```

- [ ] **Step 4: Flashen + Display prüfen**

Run: `pio run -e photopainter -t upload && pio device monitor`
Expected: 2×2-Layout, „Außen Vorne" Temp blau (4.9), andere grün, Küche „keine Daten", Header mit lokalem Wert + Akku. Schriftgrößen/Positionen ggf. justieren.

- [ ] **Step 5: Commit**

```bash
git add src/display_view.* src/main.cpp
git commit -m "feat: 2x2-Farb-Layout auf E-Paper via GxEPD2"
```

---

## Task 12: `main.cpp` — Orchestrierung + Deep-Sleep + RTC-RAM

**Files:**
- Modify: `src/main.cpp` (Endfassung)

**Interfaces:**
- Consumes: alle vorherigen Module.

- [ ] **Step 1: Endfassung schreiben**
```cpp
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

RTC_DATA_ATTR SensorReading g_prev[DEVICE_COUNT];
RTC_DATA_ATTR bool g_havePrev = false;
RTC_DATA_ATTR int  g_ntpDay   = -1;   // letzter NTP-Sync-Tag

static bool wifiConnect(uint32_t ms=15000){
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASS);
  uint32_t t0=millis(); while(WiFi.status()!=WL_CONNECTED && millis()-t0<ms) delay(200);
  return WiFi.status()==WL_CONNECTED;
}
static void maybeNtp(struct tm& now){
  if(g_ntpDay==now.tm_yday) return;                 // schon heute gesynct
  configTime(0,0,"pool.ntp.org","time.nist.gov");
  struct tm t; if(getLocalTime(&t,8000)){ rtcSet(t); g_ntpDay=t.tm_yday; now=t; }
}
static void sleepFor(uint32_t sec){
  esp_sleep_enable_timer_wakeup((uint64_t)sec*1000000ULL); esp_deep_sleep_start();
}

void setup(){
  Serial.begin(115200);
  localInit();
  // 1) lokale Werte + Zeit
  HeaderInfo hi{}; struct tm now{};
  readSHTC3(hi.localTemp,hi.localHum);
  readBattery(hi.battPct,hi.charging);
  bool haveTime = rtcNow(now);
  // 2) WLAN + ggf. NTP + Sensoren
  SensorReading cur[DEVICE_COUNT];
  hi.wifiOk = wifiConnect();
  if(hi.wifiOk){ maybeNtp(now); haveTime=rtcNow(now);
    fetchAll(DEVICE_IDS,cur,DEVICE_COUNT); }
  else { for(int i=0;i<DEVICE_COUNT;i++){ strncpy(cur[i].id,DEVICE_IDS[i],15); cur[i].valid=false; } }
  hi.hour = haveTime?now.tm_hour:12; hi.minute = haveTime?now.tm_min:0;
  // 3) WLAN-Fehler oder einzelne Sensoren leer -> letzten Stand behalten
  if(g_havePrev) for(int i=0;i<DEVICE_COUNT;i++) if(!cur[i].valid && g_prev[i].valid) cur[i]=g_prev[i];
  // 4) nur bei Aenderung zeichnen
  bool changed = !g_havePrev || anyChanged(cur,g_prev,DEVICE_COUNT);
  if(changed){ displayInit(); displayRender(cur,DEVICE_COUNT,hi); }
  for(int i=0;i<DEVICE_COUNT;i++) g_prev[i]=cur[i];
  g_havePrev=true;
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
  // 5) schlafen nach Tageszeit
  sleepFor(sleepSeconds(hi.hour));
}
void loop(){}
```

- [ ] **Step 2: Bauen**

Run: `pio run -e photopainter`
Expected: SUCCESS.

- [ ] **Step 3: Flashen + Verhalten beobachten**

Run: `pio run -e photopainter -t upload && pio device monitor`
Expected: erstes Wakeup zeichnet das Panel mit echten SwitchBot-Werten + lokalem Header; danach Deep-Sleep. Strommessung/USB: nach ~Intervall erneutes Wakeup. Bei unveränderten Werten kein erneuter Refresh (Display bleibt stehen).

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: Orchestrierung, Deep-Sleep, RTC-RAM-Vergleich, NTP-Tagessync"
```

---

## Task 13: Robustheit + Akzeptanz auf Hardware

**Files:**
- Modify: `src/main.cpp` (kleine Härtung), `src/display_view.cpp` (WLAN-Warnung)

**Interfaces:** keine neuen.

- [ ] **Step 1: WLAN-Warnsymbol im Header** in `displayRender` ergänzen (nach Header-Text):
```cpp
if(!hi.wifiOk){ display.setTextColor(GxEPD_RED); display.setCursor(770,26); display.print("!"); }
```

- [ ] **Step 2: Akzeptanztest WLAN-Ausfall**

Vorgehen: WLAN kurz sperren (falsches Passwort in `secrets.h` temporär), flashen.
Expected: letzter Stand bleibt sichtbar, rotes „!" im Header, Gerät schläft normal weiter; danach `secrets.h` zurücksetzen.

- [ ] **Step 3: Akzeptanztest „keine Änderung"**

Zweimal hintereinander Wakeup ohne Wertänderung.
Expected: zweites Wakeup zeichnet NICHT neu (kein Flackern), nur Serial-Log.

- [ ] **Step 4: Akzeptanztest Nacht-Intervall**

RTC testweise auf 02:00 setzen (per `rtcSet`), prüfen dass `sleepSeconds` 1800 wählt (Serial-Log ergänzen: `Serial.printf("sleep %us\n",sleepSeconds(hi.hour));`).
Expected: 1800.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp src/display_view.cpp
git commit -m "feat: WLAN-Warnung + Akzeptanztests Robustheit"
```

---

## Self-Review-Ergebnis (gegen Spec)

- Datenquelle SwitchBot Cloud + HMAC → Task 5, 8. ✓
- 4 Sensoren + Mapping → Task 9, gerendert Task 11. ✓
- Lokaler SHTC3 im Header → Task 10, 11. ✓
- PhotoPainter-Akku (AXP2101, mit PMIC-Vorbehalt) → Task 2, 10. ✓
- PCF85063-RTC + NTP ~1×/Tag → Task 10, 12. ✓
- Tag/Nacht-Intervall + Refresh-nur-bei-Änderung (RTC-RAM) → Task 6, 12. ✓
- 2×2 Farb-Layout, Temp-Farbe, Batterie-Warnung, „keine Daten" → Task 7, 11. ✓
- E6-Display nur Vollbild via GxEPD2 → Task 3, 11. ✓
- Fehlerbehandlung (WLAN/API/NTP/Erststart) → Task 12, 13. ✓
- Tests: Signatur, Parsing, Intervall, View-Model als native Host-Tests → Task 5–8. ✓
- Secrets nicht im Git → Task 1, 9. ✓
- v1-Abgrenzung (kein Captive-Portal/OTA/Historie/BLE) → eingehalten. ✓
```
