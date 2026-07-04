# Konfigurator-Tool + Anzeige-Modi — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Desktop-Konfigurator (tkinter) zum Einrichten und Flashen des ESP32-S3-PhotoPainter plus zwei neue Firmware-Anzeige-Modi (Stunden-Vorschau, 2×3-Thermometer-Layout).

**Architecture:** Compile-Time-Konfiguration: Das Python-Tool pflegt `tools/wetter_config.json` (Quelle der Wahrheit, gitignored), generiert daraus `include/user_config.h` + `include/secrets.h` und flasht per `python -m platformio run -e photopainter -t upload`. Die Firmware wählt das Layout per `#define DISPLAY_MODE 0|1|2` zur Compile-Zeit.

**Tech Stack:** Firmware C++ (Arduino/ESP32-S3, GxEPD2, ArduinoJson, Unity-Tests in `[env:native]`); Tool Python 3 nur mit Standardbibliothek + pyserial (kommt mit PlatformIO-Installation) + `unittest`.

**Spec:** `docs/superpowers/specs/2026-07-04-konfigurator-design.md`

## Global Constraints

- PlatformIO IMMER als `python -m platformio` aufrufen (`pio` ist nicht im PATH).
- Native-Tests: `python -m platformio test -e native` (läuft auf dem Host, kein Board nötig).
- Python-Tool: nur Standardbibliothek (+ `serial.tools.list_ports` aus der PlatformIO-Installation). KEINE pip-Installationen.
- Alle generierten/gelesenen Header sind UTF-8 (Umlaute in Namen/Titel erlaubt; Firmware rendert sie via `printUtf8`).
- `include/user_config.h`, `include/secrets.h`, `tools/wetter_config.json` sind gitignored — NIE committen. Die `*.example.h` werden gepflegt und committet.
- Der COM-Port des Boards wechselt (COM3/COM4/…) — nie hart kodieren; Erkennung über USB-VID `0x303A`.
- Commit-Messages im Repo-Stil (deutsch, `feat:`/`docs:`/`chore:`-Präfix), jeweils mit `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>` als letzter Zeile.
- Kommentar-Stil im C++-Code: deutsch, knapp, wie Bestandscode (ae/oe/ue in Kommentaren ist ok, Strings mit echten Umlauten).

---

### Task 1: Ausstehende Umlaut-Änderung committen + neue Konfig-Defines

Die Umlaut-Unterstützung (`printUtf8` in `src/display_view.cpp`, „Büro"/„Küche" in `include/user_config.h` + `include/user_config.example.h`) liegt bereits fertig und gebaut im Arbeitsverzeichnis — sie wird hier zuerst committet. Danach kommen die neuen Compile-Time-Schalter dazu.

**Files:**
- Modify: `include/config.h` (Defaults + `FORECAST_HOURS`)
- Modify: `include/user_config.example.h` (neue Defines dokumentieren)
- Commit (bereits geändert): `src/display_view.cpp`, `include/user_config.example.h`

**Interfaces:**
- Produces: `DISPLAY_MODE` (0=Tagesleiste, 1=Stundenleiste, 2=nur Thermometer), `HEADER_TITLE` (UTF-8-String), `FORECAST_HOURS` (=8) — für alle Folge-Tasks.
- Produces: `printUtf8(const char* s, uint16_t color)` existiert bereits als static-Helfer in `src/display_view.cpp` (zeichnet UTF-8-Umlaute auf ASCII-Fonts).

- [ ] **Step 1: Umlaut-Änderung committen**

```powershell
git add src/display_view.cpp include/user_config.example.h
git commit -m @'
feat: Umlaute auf dem Display (printUtf8 zeichnet Punkte auf ASCII-Glyphen)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

(`include/user_config.h` ist gitignored und bleibt lokal geändert — das ist richtig so.)

- [ ] **Step 2: Defaults + FORECAST_HOURS in `include/config.h`**

Am Dateiende (nach `#include "user_config.h"`, damit user_config Vorrang hat) ergänzen; außerdem bei `FORECAST_DAYS` die neue Konstante:

```c
// --- Vorhersage: Anzahl Tage/Stunden (Standort/Geraete in user_config.h) ---
#define FORECAST_DAYS  7
#define FORECAST_HOURS 8

// Persoenliche Konfiguration (Geraete-MACs, Standort) -> nicht im Repo.
// include/user_config.h aus user_config.example.h erstellen und ausfuellen.
#include "user_config.h"

// Defaults, falls eine aeltere user_config.h die neuen Schalter nicht kennt.
// DISPLAY_MODE: 0=Tages-Vorschau, 1=Stunden-Vorschau, 2=6 Thermometer ohne Leiste
#ifndef DISPLAY_MODE
#define DISPLAY_MODE 0
#endif
#ifndef HEADER_TITLE
#define HEADER_TITLE "SwitchBot Wetter"
#endif
```

- [ ] **Step 3: `include/user_config.example.h` erweitern**

Nach den Standort-Defines anfügen:

```c
// --- Anzeige ---
// 0 = 4 Thermometer + Tages-Vorschau, 1 = 4 Thermometer + Stunden-Vorschau,
// 2 = 6 Thermometer (2x3) ohne Wetterleiste
#define DISPLAY_MODE 0
#define HEADER_TITLE "SwitchBot Wetter"
```

- [ ] **Step 4: Build prüfen**

Run: `python -m platformio run -e photopainter`
Expected: `SUCCESS`

- [ ] **Step 5: Commit**

```powershell
git add include/config.h include/user_config.example.h
git commit -m @'
feat: Compile-Time-Schalter DISPLAY_MODE/HEADER_TITLE + FORECAST_HOURS

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 2: Stunden-Vorhersage-Parser (TDD, native)

**Files:**
- Modify: `src/forecast.h`
- Modify: `src/forecast.cpp`
- Test: `test/test_forecast/test_forecast.cpp`, `test/test_forecast/main.cpp`

**Interfaces:**
- Consumes: `FORECAST_HOURS` (=8, aus `include/config.h` — nur im ARDUINO-Teil; der Parser bleibt host-baubar ohne user_config.h).
- Produces:
  - `struct HourForecast { int hour; int wmoCode; int temp; float precipMm; bool valid; };`
  - `bool parseHourlyJson(const char* json, HourForecast* out, int maxHours, int* outCount);`
  - `int fetchHourlyForecast(HourForecast* out, int maxHours);` (Gerät: HTTPS; Host-Stub: 0)

- [ ] **Step 1: Failing Tests schreiben**

In `test/test_forecast/test_forecast.cpp` anfügen:

```cpp
void test_parse_hourly() {
  const char* body = R"({"hourly":{"time":["2026-07-04T14:00","2026-07-04T15:00"],
    "weather_code":[2,61],"temperature_2m":[27.6,26.2],
    "precipitation":[0.0,1.4]}})";
  HourForecast h[8];
  int count = 0;
  TEST_ASSERT_TRUE(parseHourlyJson(body, h, 8, &count));
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_EQUAL_INT(14, h[0].hour);
  TEST_ASSERT_EQUAL_INT(28, h[0].temp);          // 27.6 gerundet
  TEST_ASSERT_EQUAL(ICON_PARTLY, wmoToIcon(h[0].wmoCode));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, h[0].precipMm);
  TEST_ASSERT_EQUAL_INT(15, h[1].hour);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.4f, h[1].precipMm);
  TEST_ASSERT_EQUAL(ICON_RAIN, wmoToIcon(h[1].wmoCode));
}

void test_parse_hourly_bad() {
  HourForecast h[8]; int count = -1;
  TEST_ASSERT_FALSE(parseHourlyJson("{\"foo\":1}", h, 8, &count));
  TEST_ASSERT_EQUAL_INT(0, count);
}
```

In `test/test_forecast/main.cpp` deklarieren und registrieren:

```cpp
void test_parse_hourly();
void test_parse_hourly_bad();
// in main() vor UNITY_END():
  RUN_TEST(test_parse_hourly);
  RUN_TEST(test_parse_hourly_bad);
```

- [ ] **Step 2: Tests laufen lassen — müssen scheitern**

Run: `python -m platformio test -e native -f test_forecast`
Expected: Compile-FAIL („HourForecast … not declared")

- [ ] **Step 3: Implementierung**

`src/forecast.h` — nach `DayForecast` einfügen:

```cpp
struct HourForecast {
  int   hour;      // 0..23 (lokale Zeit lt. API-Timezone)
  int   wmoCode;   // Open-Meteo WMO weather code
  int   temp;      // gerundet °C
  float precipMm;  // Niederschlag mm in dieser Stunde
  bool  valid;
};
```

und nach `parseForecastJson`/`fetchForecast` deklarieren:

```cpp
// Parst Open-Meteo /v1/forecast (hourly). Fuellt bis maxHours Eintraege.
bool parseHourlyJson(const char* json, HourForecast* out, int maxHours, int* outCount);

// Geraet: holt die naechsten Stunden per HTTPS (forecast_hours). Anzahl zurueck.
int fetchHourlyForecast(HourForecast* out, int maxHours);
```

`src/forecast.cpp` — nach `parseForecastJson` einfügen:

```cpp
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
  if (time.isNull() || code.isNull() || temp.isNull() || prec.isNull()) return false;
  int n = 0;
  for (int i = 0; i < (int)time.size() && n < maxHours; i++) {
    const char* ts = time[i];              // "YYYY-MM-DDTHH:MM"
    out[n].hour = ts ? atoi(ts + 11) : 0;
    out[n].wmoCode = code[i].as<int>();
    out[n].temp = (int)lround(temp[i].as<float>());
    out[n].precipMm = prec[i].as<float>();
    out[n].valid = true;
    n++;
  }
  if (outCount) *outCount = n;
  return n > 0;
}
```

Im `#if defined(ARDUINO)`-Block nach `fetchForecast` einfügen (nutzt `forecast_hours` — Open-Meteo liefert damit ab der aktuellen Stunde):

```cpp
int fetchHourlyForecast(HourForecast* out, int maxHours) {
  for (int i = 0; i < maxHours; i++) out[i].valid = false;
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + FORECAST_LAT +
               "&longitude=" + FORECAST_LON +
               "&hourly=temperature_2m,weather_code,precipitation" +
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
```

Im `#else`-Block (Host-Stub) ergänzen:

```cpp
int fetchHourlyForecast(HourForecast*, int) { return 0; }
```

- [ ] **Step 4: Tests laufen lassen — müssen bestehen**

Run: `python -m platformio test -e native -f test_forecast`
Expected: alle Tests PASS (auch die bestehenden vier)

- [ ] **Step 5: Commit**

```powershell
git add src/forecast.h src/forecast.cpp test/test_forecast/
git commit -m @'
feat: Stunden-Vorhersage (Open-Meteo hourly) - Parser + Fetch, Unity-Tests

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 3: Anzeige-Modi im Display (Stundenleiste, 2×3-Layout, Titel)

**Files:**
- Modify: `src/display_view.h`
- Modify: `src/display_view.cpp`
- Modify: `src/displaytest.cpp:36` (Signatur-Anpassung)

**Interfaces:**
- Consumes: `HourForecast`, `FORECAST_HOURS`, `DISPLAY_MODE`, `HEADER_TITLE`, `printUtf8` (Task 1/2).
- Produces: `void displayRender(const SensorReading* r, int n, const HeaderInfo& h, const DayForecast* fc, int fcCount, const HourForecast* hf, int hfCount);` — Aufrufer übergeben für unbenutzte Leisten `nullptr, 0`.

- [ ] **Step 1: Signatur erweitern**

`src/display_view.h`:

```cpp
void displayRender(const SensorReading* r, int n, const HeaderInfo& h,
                   const DayForecast* fc, int fcCount,
                   const HourForecast* hf, int hfCount);
```

`src/displaytest.cpp:36`:

```cpp
  displayRender(demo, 4, h, fc, 7, nullptr, 0);
```

- [ ] **Step 2: `drawHourlyBar` in `src/display_view.cpp`**

Nach `drawForecastBar` einfügen:

```cpp
// Stundenleiste: pro Spalte Uhrzeit, Icon, Temperatur (Gradring), Niederschlag mm.
static void drawHourlyBar(int y0, const HourForecast* hf, int count) {
  display.drawLine(0, y0, 800, y0, GxEPD_BLACK);
  int cols = count < FORECAST_HOURS ? count : FORECAST_HOURS;
  if (cols <= 0) return;
  int colW = 800 / cols;
  for (int i = 0; i < cols; i++) {
    int cx = i * colW + colW / 2;
    if (i > 0) display.drawLine(i * colW, y0 + 6, i * colW, y0 + 90, GxEPD_BLACK);
    char hbuf[8];
    snprintf(hbuf, sizeof hbuf, "%02d:00", hf[i].hour);
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(i * colW + 6, y0 + 22);
    display.print(hbuf);
    drawWeatherIcon(cx, y0 + 46, wmoToIcon(hf[i].wmoCode));
    // Temperatur links unten (Gradring wie Tagesleiste), Niederschlag blau daneben
    char ts[8];
    snprintf(ts, sizeof ts, "%d", hf[i].temp);
    display.setCursor(i * colW + 6, y0 + 90);
    display.print(ts);
    int dx = display.getCursorX();
    display.drawCircle(dx + 3, y0 + 78, 2, GxEPD_BLACK);
    char ps[12];
    if (hf[i].precipMm >= 0.05f) snprintf(ps, sizeof ps, "%.1f mm", hf[i].precipMm);
    else snprintf(ps, sizeof ps, "0 mm");
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLUE);
    display.setCursor(dx + 12, y0 + 90);
    display.print(ps);
  }
}
```

- [ ] **Step 3: `displayRender` umbauen**

Titel, Kachelraster und Leiste modusabhängig machen. Die betroffenen Stellen in `displayRender` ersetzen:

```cpp
void displayRender(const SensorReading* r, int n, const HeaderInfo& hi,
                   const DayForecast* fc, int fcCount,
                   const HourForecast* hf, int hfCount) {
  (void)fc; (void)fcCount; (void)hf; (void)hfCount;  // je nach Modus unbenutzt
```

Titel (bisher `display.print("SwitchBot Wetter");`):

```cpp
    display.setCursor(12, 40);
    printUtf8(HEADER_TITLE, GxEPD_BLACK);
```

Kachelraster + Leiste (bisher fixes `FC_Y = 384`, 2×2, `drawForecastBar`):

```cpp
    // --- Kachelraster: 2x2 mit Wetterleiste, 2x3 ohne (Modus 2) ---
#if DISPLAY_MODE == 2
    const int FC_Y = 480;
    const int ROWS = 3;
#else
    const int FC_Y = 384;
    const int ROWS = 2;
#endif
    int gx = 8, gy = 88, gw = (800 - 24) / 2, gh = (FC_Y - 88 - 4 * (ROWS - 1)) / ROWS;
    int pos = 0;
    for (int row = 0; row < ROWS && pos < n; row++)
      for (int col = 0; col < 2 && pos < n; col++, pos++)
        drawTile(gx + col * (gw + 8), gy + row * (gh + 4), gw, gh, DEVICE_NAMES[pos], r[pos]);
    // --- Wetterleiste je nach Modus ---
#if DISPLAY_MODE == 0
    drawForecastBar(FC_Y, fc, fcCount);
#elif DISPLAY_MODE == 1
    drawHourlyBar(FC_Y, hf, hfCount);
#endif
```

Hinweis: In `drawTile` prüfen, dass die Inhalte bei `gh ≈ 128` (Modus 2) nicht überlaufen — die y-Offsets (30/96/128) passen gerade noch; falls der Akku-Text (y+128) die Kachelunterkante berührt, Offset auf `h - 10` relativieren:

```cpp
    display.setCursor(x + 18, y + h - 14);   // statt y + 128
```

(Diese Relativierung gleich mit umsetzen — sie ist auch für 2×2 identisch: 146-14≈132 vs. 128, minimale Verschiebung.)

- [ ] **Step 4: Build in allen drei Modi prüfen**

In `include/user_config.h` temporär `#define DISPLAY_MODE 0` ergänzen (steht dort noch nicht), bauen; dann auf `1` und `2` ändern, jeweils bauen. Für Modus 2 zusätzlich prüfen, dass `DEVICE_COUNT 4` weiter kompiliert (weniger Kacheln als Plätze ist erlaubt).

Run (3×): `python -m platformio run -e photopainter`
Expected: 3× `SUCCESS`. Danach `DISPLAY_MODE 0` in `user_config.h` stehen lassen.

Zusätzlich: `python -m platformio run -e displaytest`
Expected: `SUCCESS` (Signatur-Anpassung greift)

- [ ] **Step 5: Commit**

```powershell
git add src/display_view.h src/display_view.cpp src/displaytest.cpp
git commit -m @'
feat: drei Anzeige-Modi - Stundenleiste, 2x3-Kacheln, konfigurierbarer Titel

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 4: main.cpp — modusabhängiger Datenabruf + RTC-Cache

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `fetchHourlyForecast`, `HourForecast`, `FORECAST_HOURS`, neue `displayRender`-Signatur.

- [ ] **Step 1: RTC-Variablen + Abruf umbauen**

Nach den bestehenden RTC-Variablen (`g_prevFc…`) ergänzen:

```cpp
RTC_DATA_ATTR HourForecast g_prevHf[FORECAST_HOURS];
RTC_DATA_ATTR int  g_prevHfCount = 0;
```

In `setup()` die Vorhersage-Teile ersetzen. Deklaration:

```cpp
  SensorReading cur[DEVICE_COUNT];
  DayForecast fc[FORECAST_DAYS];
  int fcCount = 0;
  HourForecast hf[FORECAST_HOURS];
  int hfCount = 0;
```

Abruf (bisher nur `fcCount = fetchForecast(…)`):

```cpp
    fetchAll(DEVICE_IDS, cur, DEVICE_COUNT);
#if DISPLAY_MODE == 0
    fcCount = fetchForecast(fc, FORECAST_DAYS);
#elif DISPLAY_MODE == 1
    hfCount = fetchHourlyForecast(hf, FORECAST_HOURS);
#endif
```

Fallback auf letzten Stand (nach dem bestehenden `fc`-Fallback):

```cpp
  if (hfCount == 0 && g_prevHfCount > 0) {
    for (int i = 0; i < g_prevHfCount; i++) hf[i] = g_prevHf[i];
    hfCount = g_prevHfCount;
  }
```

Änderungs-Erkennung erweitern (nach der `fcChanged`-Schleife):

```cpp
  bool hfChanged = (hfCount != g_prevHfCount);
  for (int i = 0; i < hfCount && !hfChanged; i++)
    if (hf[i].hour != g_prevHf[i].hour || hf[i].wmoCode != g_prevHf[i].wmoCode ||
        hf[i].temp != g_prevHf[i].temp ||
        fabsf(hf[i].precipMm - g_prevHf[i].precipMm) > 0.05f)
      hfChanged = true;
  bool changed = !g_havePrev || anyChanged(cur, g_prev, DEVICE_COUNT) || fcChanged || hfChanged;
```

(`#include <math.h>` oben ergänzen, falls `fabsf` fehlt.)

Render-Aufruf:

```cpp
    displayRender(cur, DEVICE_COUNT, hi, fc, fcCount, hf, hfCount);
```

Stand sichern (nach `g_prevFcCount = fcCount;`):

```cpp
  for (int i = 0; i < hfCount; i++) g_prevHf[i] = hf[i];
  g_prevHfCount = hfCount;
```

- [ ] **Step 2: Build in allen drei Modi**

Wie Task 3 Step 4: `DISPLAY_MODE` in `include/user_config.h` auf 0/1/2 durchschalten.

Run (3×): `python -m platformio run -e photopainter`
Expected: 3× `SUCCESS`; danach Modus wieder auf 0.

- [ ] **Step 3: Native-Tests als Regressionscheck**

Run: `python -m platformio test -e native`
Expected: alle Testsuites PASS

- [ ] **Step 4: Commit**

```powershell
git add src/main.cpp
git commit -m @'
feat: main laedt Tages- oder Stundenvorhersage je nach DISPLAY_MODE (RTC-Cache)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 5: Tool — Konfigurationsspeicher + Header-Generierung (`tools/config_store.py`)

**Files:**
- Create: `tools/config_store.py`
- Test: `tools/test_config_store.py`
- Modify: `.gitignore` (+ `tools/wetter_config.json`)

**Interfaces:**
- Produces:
  - `DEFAULTS: dict` — Grundzustand
  - `load(json_path=None, user_config_path=None, secrets_path=None) -> dict`
  - `save(cfg: dict, json_path=None, user_config_path=None, secrets_path=None) -> None` (schreibt JSON UND beide Header)
  - `selected_devices(cfg) -> list[dict]`, `device_limit(mode: int) -> int` (4 bzw. 6)
  - JSON-Schema: `{"header_title": str, "display_mode": 0|1|2, "wifi_ssid": str, "wifi_pass": str, "sb_token": str, "sb_secret": str, "lat": str, "lon": str, "tz": str, "devices": [{"id": str, "name": str, "outdoor": bool, "selected": bool}]}`

- [ ] **Step 1: Failing Tests schreiben — `tools/test_config_store.py`**

```python
import tempfile, unittest
from pathlib import Path

import config_store as cs


def make_cfg():
    return {
        "header_title": "Wetter Büro",
        "display_mode": 1,
        "wifi_ssid": "MeinWLAN",
        "wifi_pass": 'pass"wort\\x',
        "sb_token": "tok",
        "sb_secret": "sec",
        "lat": "48.5333",
        "lon": "15.9167",
        "tz": "Europe/Vienna",
        "devices": [
            {"id": "AAAA", "name": "Büro", "outdoor": False, "selected": True},
            {"id": "BBBB", "name": "Garten", "outdoor": True, "selected": True},
            {"id": "CCCC", "name": "Keller", "outdoor": False, "selected": False},
        ],
    }


class TestConfigStore(unittest.TestCase):
    def test_device_limit(self):
        self.assertEqual(cs.device_limit(0), 4)
        self.assertEqual(cs.device_limit(1), 4)
        self.assertEqual(cs.device_limit(2), 6)

    def test_selected_devices(self):
        sel = cs.selected_devices(make_cfg())
        self.assertEqual([d["id"] for d in sel], ["AAAA", "BBBB"])

    def test_save_and_load_roundtrip(self):
        with tempfile.TemporaryDirectory() as td:
            jp = Path(td) / "cfg.json"
            uc = Path(td) / "user_config.h"
            sh = Path(td) / "secrets.h"
            cfg = make_cfg()
            cs.save(cfg, json_path=jp, user_config_path=uc, secrets_path=sh)
            self.assertEqual(cs.load(json_path=jp), cfg)
            text = uc.read_text(encoding="utf-8")
            self.assertIn('#define DISPLAY_MODE 1', text)
            self.assertIn('#define HEADER_TITLE "Wetter Büro"', text)
            self.assertIn('#define DEVICE_COUNT 2', text)   # nur selektierte
            self.assertIn('{"AAAA","BBBB"}', text.replace("\n  ", ""))
            self.assertIn('{"Büro","Garten"}', text.replace("\n  ", ""))
            self.assertIn('{false,true}', text.replace("\n  ", ""))
            self.assertIn('#define FORECAST_LAT  "48.5333"', text)
            stext = sh.read_text(encoding="utf-8")
            self.assertIn('#define WIFI_SSID  "MeinWLAN"', stext)
            # Escaping: " -> \" und \ -> \\
            self.assertIn('#define WIFI_PASS  "pass\\"wort\\\\x"', stext)

    def test_load_parses_existing_headers(self):
        with tempfile.TemporaryDirectory() as td:
            jp = Path(td) / "cfg.json"   # existiert nicht
            uc = Path(td) / "user_config.h"
            sh = Path(td) / "secrets.h"
            uc.write_text(
                '#define DEVICE_COUNT 2\n'
                'static const char* const DEVICE_IDS[DEVICE_COUNT] =\n'
                '  {"X1","X2"};\n'
                'static const char* const DEVICE_NAMES[DEVICE_COUNT] =\n'
                '  {"Büro","Küche"};\n'
                'static const bool DEVICE_OUTDOOR[DEVICE_COUNT] = {false,true};\n'
                '#define FORECAST_LAT  "48.1"\n'
                '#define FORECAST_LON  "16.2"\n'
                '#define FORECAST_TZ   "Europe/Vienna"\n',
                encoding="utf-8")
            sh.write_text('#define WIFI_SSID  "W"\n#define WIFI_PASS  "P"\n'
                          '#define SB_TOKEN   "T"\n#define SB_SECRET  "S"\n',
                          encoding="utf-8")
            cfg = cs.load(json_path=jp, user_config_path=uc, secrets_path=sh)
            self.assertEqual(cfg["wifi_ssid"], "W")
            self.assertEqual(cfg["lat"], "48.1")
            self.assertEqual(cfg["display_mode"], 0)          # Default
            self.assertEqual(cfg["header_title"], "SwitchBot Wetter")  # Default
            self.assertEqual(len(cfg["devices"]), 2)
            self.assertEqual(cfg["devices"][1],
                             {"id": "X2", "name": "Küche", "outdoor": True, "selected": True})

    def test_load_without_anything_returns_defaults(self):
        with tempfile.TemporaryDirectory() as td:
            cfg = cs.load(json_path=Path(td) / "x.json",
                          user_config_path=Path(td) / "y.h",
                          secrets_path=Path(td) / "z.h")
            self.assertEqual(cfg["display_mode"], 0)
            self.assertEqual(cfg["devices"], [])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Tests laufen lassen — müssen scheitern**

Run: `python -m unittest discover -s tools -p "test_*.py" -v`
Expected: FAIL/ERROR („No module named 'config_store'")

- [ ] **Step 3: Implementierung — `tools/config_store.py`**

```python
"""Konfiguration des Wetter-Displays: JSON laden/speichern, C-Header generieren.

Quelle der Wahrheit ist tools/wetter_config.json (gitignored). Beim Speichern
werden include/user_config.h und include/secrets.h daraus generiert.
Existiert noch kein JSON, werden vorhandene Header als Vorbelegung geparst.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "tools" / "wetter_config.json"
USER_CONFIG_H = ROOT / "include" / "user_config.h"
SECRETS_H = ROOT / "include" / "secrets.h"

MODE_NAMES = ["Tages-Vorschau", "Stunden-Vorschau", "6 Thermometer"]

DEFAULTS = {
    "header_title": "SwitchBot Wetter",
    "display_mode": 0,
    "wifi_ssid": "",
    "wifi_pass": "",
    "sb_token": "",
    "sb_secret": "",
    "lat": "48.0000",
    "lon": "16.0000",
    "tz": "Europe/Vienna",
    "devices": [],
}


def device_limit(mode):
    return 6 if mode == 2 else 4


def selected_devices(cfg):
    return [d for d in cfg["devices"] if d.get("selected")]


def _c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _defines(text):
    """#define NAME "wert" -> dict (nur String-Defines)."""
    return dict(re.findall(r'#define\s+(\w+)\s+"((?:[^"\\]|\\.)*)"', text))


def _unescape(s):
    return s.replace('\\"', '"').replace("\\\\", "\\")


def _str_list(text, name):
    """Werte aus  static ... NAME[...] = {"a","b"};  extrahieren."""
    m = re.search(re.escape(name) + r"\[[^\]]*\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    if not m:
        return []
    return [_unescape(v) for v in re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))]


def _bool_list(text, name):
    m = re.search(re.escape(name) + r"\[[^\]]*\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    if not m:
        return []
    return [w.strip() == "true" for w in m.group(1).split(",") if w.strip()]


def _parse_headers(user_config_path, secrets_path):
    cfg = dict(DEFAULTS, devices=[])
    if user_config_path.exists():
        text = user_config_path.read_text(encoding="utf-8")
        d = _defines(text)
        cfg["lat"] = d.get("FORECAST_LAT", cfg["lat"])
        cfg["lon"] = d.get("FORECAST_LON", cfg["lon"])
        cfg["tz"] = d.get("FORECAST_TZ", cfg["tz"])
        cfg["header_title"] = _unescape(d.get("HEADER_TITLE", cfg["header_title"]))
        m = re.search(r"#define\s+DISPLAY_MODE\s+(\d)", text)
        if m:
            cfg["display_mode"] = int(m.group(1))
        ids = _str_list(text, "DEVICE_IDS")
        names = _str_list(text, "DEVICE_NAMES")
        outs = _bool_list(text, "DEVICE_OUTDOOR")
        for i, dev_id in enumerate(ids):
            cfg["devices"].append({
                "id": dev_id,
                "name": names[i] if i < len(names) else dev_id,
                "outdoor": outs[i] if i < len(outs) else False,
                "selected": True,
            })
    if secrets_path.exists():
        d = _defines(secrets_path.read_text(encoding="utf-8"))
        cfg["wifi_ssid"] = _unescape(d.get("WIFI_SSID", ""))
        cfg["wifi_pass"] = _unescape(d.get("WIFI_PASS", ""))
        cfg["sb_token"] = _unescape(d.get("SB_TOKEN", ""))
        cfg["sb_secret"] = _unescape(d.get("SB_SECRET", ""))
    return cfg


def load(json_path=None, user_config_path=None, secrets_path=None):
    json_path = Path(json_path or JSON_PATH)
    if json_path.exists():
        cfg = dict(DEFAULTS)
        cfg.update(json.loads(json_path.read_text(encoding="utf-8")))
        return cfg
    return _parse_headers(Path(user_config_path or USER_CONFIG_H),
                          Path(secrets_path or SECRETS_H))


def _gen_user_config(cfg):
    sel = selected_devices(cfg)
    ids = ",".join(_c_str(d["id"]) for d in sel)
    names = ",".join(_c_str(d["name"]) for d in sel)
    outs = ",".join("true" if d["outdoor"] else "false" for d in sel)
    return (
        "#pragma once\n"
        "// Von tools/configurator.py generiert - nicht von Hand editieren, nicht committen.\n\n"
        f"#define DEVICE_COUNT {len(sel)}\n"
        "static const char* const DEVICE_IDS[DEVICE_COUNT] =\n"
        f"  {{{ids}}};\n"
        "static const char* const DEVICE_NAMES[DEVICE_COUNT] =\n"
        f"  {{{names}}};\n"
        f"static const bool DEVICE_OUTDOOR[DEVICE_COUNT] = {{{outs}}};\n\n"
        f'#define FORECAST_LAT  {_c_str(cfg["lat"])}\n'
        f'#define FORECAST_LON  {_c_str(cfg["lon"])}\n'
        f'#define FORECAST_TZ   {_c_str(cfg["tz"])}\n\n'
        f'#define DISPLAY_MODE {cfg["display_mode"]}\n'
        f'#define HEADER_TITLE {_c_str(cfg["header_title"])}\n'
    )


def _gen_secrets(cfg):
    return (
        "#pragma once\n"
        "// Von tools/configurator.py generiert - nicht committen.\n"
        f'#define WIFI_SSID  {_c_str(cfg["wifi_ssid"])}\n'
        f'#define WIFI_PASS  {_c_str(cfg["wifi_pass"])}\n'
        f'#define SB_TOKEN   {_c_str(cfg["sb_token"])}\n'
        f'#define SB_SECRET  {_c_str(cfg["sb_secret"])}\n'
    )


def save(cfg, json_path=None, user_config_path=None, secrets_path=None):
    Path(json_path or JSON_PATH).write_text(
        json.dumps(cfg, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    Path(user_config_path or USER_CONFIG_H).write_text(_gen_user_config(cfg), encoding="utf-8")
    Path(secrets_path or SECRETS_H).write_text(_gen_secrets(cfg), encoding="utf-8")
```

- [ ] **Step 4: Tests laufen lassen — müssen bestehen**

Run: `python -m unittest discover -s tools -p "test_*.py" -v`
Expected: alle PASS

- [ ] **Step 5: `.gitignore` ergänzen**

Zeile anfügen: `tools/wetter_config.json`

- [ ] **Step 6: Commit**

```powershell
git add tools/config_store.py tools/test_config_store.py .gitignore
git commit -m @'
feat: Konfigurator-Basis - JSON-Store + Header-Generierung mit Tests

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 6: Tool — SwitchBot-API-Client (`tools/sb_api.py`)

**Files:**
- Create: `tools/sb_api.py`
- Test: `tools/test_sb_api.py`

**Interfaces:**
- Consumes: nichts aus anderen Tasks (Standardbibliothek).
- Produces:
  - `sign_headers(token: str, secret: str, t: str|None = None, nonce: str|None = None) -> dict` — HTTP-Header inkl. HMAC-SHA256-Signatur (Schema wie `src/sb_sign.cpp`)
  - `is_meter(device_type: str) -> bool`
  - `list_meters(token: str, secret: str, timeout: int = 15) -> list[dict]` — `[{"id", "name", "type"}]`, wirft `RuntimeError` mit verständlicher Meldung bei API-/Netzfehler

- [ ] **Step 1: Failing Tests schreiben — `tools/test_sb_api.py`**

(Der Vergleichswert in `test_sign_headers_deterministic` wurde vorab berechnet mit
`python -c "import base64,hashlib,hmac;print(base64.b64encode(hmac.new(b'sec',b'tok'+b'1000'+b'nonce1',hashlib.sha256).digest()).decode())"`.)

```python
import unittest

import sb_api


class TestSbApi(unittest.TestCase):
    def test_sign_headers_deterministic(self):
        h = sb_api.sign_headers("tok", "sec", t="1000", nonce="nonce1")
        self.assertEqual(h["Authorization"], "tok")
        self.assertEqual(h["t"], "1000")
        self.assertEqual(h["nonce"], "nonce1")
        self.assertEqual(h["sign"], "O4Jkd6KlptflxgMcPzDRiTTRCNJVwx8zq7v1ITxJZJQ=")

    def test_sign_headers_autofills(self):
        h = sb_api.sign_headers("tok", "sec")
        self.assertTrue(h["t"].isdigit())
        self.assertTrue(len(h["nonce"]) >= 8)
        self.assertTrue(len(h["sign"]) >= 40)

    def test_is_meter(self):
        for t in ("Meter", "MeterPlus", "MeterPro", "MeterPro(CO2)",
                  "WoIOSensor", "Hub 2"):
            self.assertTrue(sb_api.is_meter(t), t)
        for t in ("Bot", "Curtain", "Plug Mini (JP)", ""):
            self.assertFalse(sb_api.is_meter(t), t)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Tests laufen lassen — müssen scheitern**

Run: `python -m unittest discover -s tools -p "test_*.py" -v`
Expected: ERROR („No module named 'sb_api'"); config_store-Tests weiter PASS

- [ ] **Step 3: Implementierung — `tools/sb_api.py`**

```python
"""SwitchBot Cloud API v1.1: Signatur + Geraeteliste (nur Thermo-/Hygrometer)."""
import base64
import hashlib
import hmac
import json
import time
import urllib.error
import urllib.request
import uuid

API_BASE = "https://api.switch-bot.com"


def sign_headers(token, secret, t=None, nonce=None):
    t = t if t is not None else str(int(time.time() * 1000))
    nonce = nonce if nonce is not None else str(uuid.uuid4())
    msg = (token + t + nonce).encode()
    sig = base64.b64encode(
        hmac.new(secret.encode(), msg, hashlib.sha256).digest()).decode()
    return {"Authorization": token, "sign": sig, "t": t, "nonce": nonce,
            "Content-Type": "application/json"}


def is_meter(device_type):
    return "Meter" in device_type or device_type in ("WoIOSensor", "Hub 2")


def list_meters(token, secret, timeout=15):
    req = urllib.request.Request(API_BASE + "/v1.1/devices",
                                 headers=sign_headers(token, secret))
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            data = json.load(r)
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"SwitchBot-API HTTP {e.code} - Token/Secret pruefen.") from e
    except OSError as e:
        raise RuntimeError(f"Keine Verbindung zur SwitchBot-API: {e}") from e
    if data.get("statusCode") != 100:
        raise RuntimeError(f"SwitchBot-API Fehler: {data.get('message', data)}")
    return [{"id": d["deviceId"],
             "name": d.get("deviceName") or d["deviceId"],
             "type": d.get("deviceType", "")}
            for d in data.get("body", {}).get("deviceList", [])
            if is_meter(d.get("deviceType", ""))]
```

- [ ] **Step 4: Tests laufen lassen — müssen bestehen**

Run: `python -m unittest discover -s tools -p "test_*.py" -v`
Expected: alle PASS

- [ ] **Step 5: Live-Check gegen die echte API**

Run: `python -c "import sys; sys.path.insert(0,'tools'); import sb_api, config_store; c=config_store.load(); print(sb_api.list_meters(c['sb_token'], c['sb_secret']))"`
Expected: Liste mit den 4 bekannten Thermometern (Aussen Hinten/Vorne, Büro, Küche) — Namen wie in der SwitchBot-App.

- [ ] **Step 6: Commit**

```powershell
git add tools/sb_api.py tools/test_sb_api.py
git commit -m @'
feat: SwitchBot-API-Client (HMAC-Signatur, Geraeteliste) fuer den Konfigurator

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 7: Tool — Port-Erkennung + Flash (`tools/flasher.py`)

**Files:**
- Create: `tools/flasher.py`
- Test: `tools/test_flasher.py`

**Interfaces:**
- Consumes: nichts aus anderen Tasks.
- Produces:
  - `ESP32S3_VID = 0x303A`
  - `pick_port(ports) -> str|None` — pur, testbar (nimmt Objekte mit `.vid`/`.device`)
  - `find_port() -> str|None` — scannt echte COM-Ports (pyserial)
  - `check_connection(port: str) -> tuple[bool, str]` — esptool `read_mac`
  - `upload(port: str, on_line: callable) -> bool` — PlatformIO-Upload, streamt Ausgabezeilen
  - `PORT_HELP: str` — Anleitungstext (PWR-Taste, Download-Modus)

- [ ] **Step 1: Failing Tests schreiben — `tools/test_flasher.py`**

```python
import unittest
from types import SimpleNamespace

import flasher


class TestFlasher(unittest.TestCase):
    def test_pick_port_finds_esp32s3(self):
        ports = [SimpleNamespace(vid=0x046D, device="COM7"),
                 SimpleNamespace(vid=0x303A, device="COM4"),
                 SimpleNamespace(vid=None, device="COM1")]
        self.assertEqual(flasher.pick_port(ports), "COM4")

    def test_pick_port_none(self):
        self.assertIsNone(flasher.pick_port([]))
        self.assertIsNone(flasher.pick_port([SimpleNamespace(vid=0x1234, device="COM9")]))

    def test_help_text_mentions_pwr_and_boot(self):
        self.assertIn("PWR", flasher.PORT_HELP)
        self.assertIn("BOOT", flasher.PORT_HELP)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Tests laufen lassen — müssen scheitern**

Run: `python -m unittest discover -s tools -p "test_*.py" -v`
Expected: ERROR („No module named 'flasher'")

- [ ] **Step 3: Implementierung — `tools/flasher.py`**

```python
"""ESP32-S3-PhotoPainter finden (USB-VID 303A), pruefen und flashen."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ESP32S3_VID = 0x303A
ESPTOOL = Path.home() / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py"

PORT_HELP = (
    "Kein PhotoPainter gefunden (USB-VID 303A).\n\n"
    "1. USB-Kabel pruefen - viele Kabel sind reine Ladekabel.\n"
    "2. Das Board laeuft auf Akku weiter! Echter Neustart nur per PWR-Taste\n"
    "   (aus- und wieder einschalten).\n"
    "3. Schlaeft die Firmware sofort wieder ein (Deep-Sleep), Download-Modus\n"
    "   erzwingen: BOOT-Taste halten, dabei PWR aus/an, BOOT ~5 s weiter halten.\n"
)


def pick_port(ports):
    for p in ports:
        if getattr(p, "vid", None) == ESP32S3_VID:
            return p.device
    return None


def find_port():
    from serial.tools import list_ports  # pyserial (Teil der PlatformIO-Installation)
    return pick_port(list_ports.comports())


def check_connection(port):
    r = subprocess.run(
        [sys.executable, str(ESPTOOL), "--port", port, "--baud", "115200", "read_mac"],
        capture_output=True, text=True, timeout=90)
    return r.returncode == 0, (r.stdout or "") + (r.stderr or "")


def upload(port, on_line):
    proc = subprocess.Popen(
        [sys.executable, "-m", "platformio", "run", "-e", "photopainter",
         "-t", "upload", "--upload-port", port],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, encoding="utf-8", errors="replace")
    for line in proc.stdout:
        on_line(line.rstrip())
    return proc.wait() == 0
```

- [ ] **Step 4: Tests laufen lassen — müssen bestehen**

Run: `python -m unittest discover -s tools -p "test_*.py" -v`
Expected: alle PASS

- [ ] **Step 5: Live-Check mit angeschlossenem Board**

Run: `python -c "import sys; sys.path.insert(0,'tools'); import flasher; p=flasher.find_port(); print(p, flasher.check_connection(p) if p else 'kein Port')"`
Expected: z. B. `COM4 (True, '... MAC: e8:f6:0a:8d:bc:c8 ...')` (Port kann abweichen; ist das Board ab, kommt `None kein Port` — dann Board anstecken und wiederholen)

- [ ] **Step 6: Commit**

```powershell
git add tools/flasher.py tools/test_flasher.py
git commit -m @'
feat: Flasher-Modul - Port-Erkennung (VID 303A), esptool-Check, PIO-Upload

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 8: Tool — tkinter-GUI (`tools/configurator.py`)

**Files:**
- Create: `tools/configurator.py`

**Interfaces:**
- Consumes: `config_store.load/save/device_limit/selected_devices/MODE_NAMES`, `sb_api.list_meters`, `flasher.find_port/check_connection/upload/PORT_HELP`.
- Produces: startbares Skript `python tools/configurator.py`.

- [ ] **Step 1: GUI implementieren**

```python
"""Konfigurations- und Installations-Tool fuer den PhotoPainter-Wetter-Monitor.

Start: python tools/configurator.py
Speichern = JSON + Header generieren. Upload = Speichern + Flashen via PlatformIO.
"""
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk

import config_store as cs
import flasher
import sb_api


class App:
    def __init__(self, root):
        self.root = root
        root.title("PhotoPainter Wetter - Konfiguration")
        self.cfg = cs.load()
        self.msgq = queue.Queue()   # (typ, text) aus Worker-Threads
        self.busy = False

        main = ttk.Frame(root, padding=10)
        main.grid(sticky="nsew")
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)
        main.columnconfigure(0, weight=1)

        # --- Allgemein ---
        gen = ttk.LabelFrame(main, text="Anzeige", padding=8)
        gen.grid(sticky="ew", pady=(0, 8))
        gen.columnconfigure(1, weight=1)
        ttk.Label(gen, text="Überschrift:").grid(row=0, column=0, sticky="w")
        self.v_title = tk.StringVar(value=self.cfg["header_title"])
        ttk.Entry(gen, textvariable=self.v_title).grid(row=0, column=1, sticky="ew", padx=6)
        ttk.Label(gen, text="Modus:").grid(row=1, column=0, sticky="w")
        self.v_mode = tk.IntVar(value=self.cfg["display_mode"])
        modefrm = ttk.Frame(gen)
        modefrm.grid(row=1, column=1, sticky="w", padx=6)
        for i, name in enumerate(cs.MODE_NAMES):
            ttk.Radiobutton(modefrm, text=name, value=i, variable=self.v_mode,
                            command=self.on_mode_change).pack(side="left", padx=(0, 10))

        # --- Zugangsdaten ---
        acc = ttk.LabelFrame(main, text="Zugangsdaten", padding=8)
        acc.grid(sticky="ew", pady=(0, 8))
        acc.columnconfigure(1, weight=1)
        acc.columnconfigure(3, weight=1)
        self.v_ssid = self._entry(acc, 0, 0, "WLAN-SSID:", self.cfg["wifi_ssid"])
        self.v_pass = self._entry(acc, 0, 2, "Passwort:", self.cfg["wifi_pass"], show="*")
        self.v_token = self._entry(acc, 1, 0, "SwitchBot-Token:", self.cfg["sb_token"], show="*")
        self.v_secret = self._entry(acc, 1, 2, "Secret:", self.cfg["sb_secret"], show="*")

        # --- Standort ---
        loc = ttk.LabelFrame(main, text="Standort (Open-Meteo)", padding=8)
        loc.grid(sticky="ew", pady=(0, 8))
        for c in (1, 3, 5):
            loc.columnconfigure(c, weight=1)
        self.v_lat = self._entry(loc, 0, 0, "Breite:", self.cfg["lat"])
        self.v_lon = self._entry(loc, 0, 2, "Länge:", self.cfg["lon"])
        self.v_tz = self._entry(loc, 0, 4, "Zeitzone:", self.cfg["tz"])

        # --- Sensoren ---
        sens = ttk.LabelFrame(main, text="Thermometer", padding=8)
        sens.grid(sticky="nsew", pady=(0, 8))
        main.rowconfigure(3, weight=1)
        sens.columnconfigure(0, weight=1)
        self.dev_frame = ttk.Frame(sens)
        self.dev_frame.grid(sticky="nsew")
        btns = ttk.Frame(sens)
        btns.grid(sticky="w", pady=(6, 0))
        ttk.Button(btns, text="Sensoren laden (SwitchBot-Cloud)",
                   command=self.on_load_devices).pack(side="left")
        self.lbl_limit = ttk.Label(btns, text="")
        self.lbl_limit.pack(side="left", padx=10)
        self.render_devices()

        # --- Log + Aktionen ---
        self.log = tk.Text(main, height=10, state="disabled", wrap="none")
        self.log.grid(sticky="nsew", pady=(0, 8))
        main.rowconfigure(4, weight=1)
        act = ttk.Frame(main)
        act.grid(sticky="ew")
        self.btn_save = ttk.Button(act, text="Speichern", command=self.on_save)
        self.btn_save.pack(side="left")
        self.btn_upload = ttk.Button(act, text="Upload (Speichern + Flashen)",
                                     command=self.on_upload)
        self.btn_upload.pack(side="left", padx=8)
        self.status = ttk.Label(act, text="")
        self.status.pack(side="left", padx=10)

        self.root.after(100, self.poll_queue)

    def _entry(self, parent, row, col, label, value, show=None):
        ttk.Label(parent, text=label).grid(row=row, column=col, sticky="w")
        var = tk.StringVar(value=value)
        ttk.Entry(parent, textvariable=var, show=show).grid(
            row=row, column=col + 1, sticky="ew", padx=6, pady=2)
        return var

    # ---------- Sensorliste ----------
    def render_devices(self):
        for w in self.dev_frame.winfo_children():
            w.destroy()
        self.dev_vars = []
        for i, d in enumerate(self.cfg["devices"]):
            v_sel = tk.BooleanVar(value=d["selected"])
            v_name = tk.StringVar(value=d["name"])
            v_out = tk.BooleanVar(value=d["outdoor"])
            self.dev_vars.append((v_sel, v_name, v_out))
            ttk.Checkbutton(self.dev_frame, variable=v_sel,
                            command=lambda i=i: self.on_select(i)).grid(row=i, column=0)
            ttk.Entry(self.dev_frame, textvariable=v_name, width=24).grid(
                row=i, column=1, padx=4, pady=1)
            ttk.Checkbutton(self.dev_frame, text="Außen", variable=v_out).grid(row=i, column=2)
            offline = " (nicht in der Cloud gefunden)" if d.get("offline") else ""
            ttk.Label(self.dev_frame, text=d["id"] + offline).grid(
                row=i, column=3, sticky="w", padx=6)
            ttk.Button(self.dev_frame, text="↑", width=2,
                       command=lambda i=i: self.move(i, -1)).grid(row=i, column=4)
            ttk.Button(self.dev_frame, text="↓", width=2,
                       command=lambda i=i: self.move(i, +1)).grid(row=i, column=5)
        self.update_limit_label()

    def sync_devices(self):
        for d, (v_sel, v_name, v_out) in zip(self.cfg["devices"], self.dev_vars):
            d["selected"] = v_sel.get()
            d["name"] = v_name.get().strip() or d["id"]
            d["outdoor"] = v_out.get()

    def update_limit_label(self):
        limit = cs.device_limit(self.v_mode.get())
        count = sum(1 for v_sel, _, _ in self.dev_vars if v_sel.get())
        self.lbl_limit.config(text=f"{count}/{limit} ausgewählt")

    def on_select(self, i):
        limit = cs.device_limit(self.v_mode.get())
        count = sum(1 for v_sel, _, _ in self.dev_vars if v_sel.get())
        if count > limit:
            self.dev_vars[i][0].set(False)
            messagebox.showwarning(
                "Limit", f"In diesem Modus sind maximal {limit} Thermometer möglich.")
        self.update_limit_label()

    def on_mode_change(self):
        limit = cs.device_limit(self.v_mode.get())
        count = sum(1 for v_sel, _, _ in self.dev_vars if v_sel.get())
        if count > limit:
            messagebox.showwarning(
                "Limit", f"Es sind {count} Thermometer gewählt, der Modus erlaubt {limit}.\n"
                         "Bitte Auswahl reduzieren.")
        self.update_limit_label()

    def move(self, i, delta):
        j = i + delta
        if 0 <= j < len(self.cfg["devices"]):
            self.sync_devices()
            devs = self.cfg["devices"]
            devs[i], devs[j] = devs[j], devs[i]
            self.render_devices()

    def on_load_devices(self):
        self.sync_devices()
        token, secret = self.v_token.get().strip(), self.v_secret.get().strip()
        if not token or not secret:
            messagebox.showerror("Fehlt", "SwitchBot-Token und -Secret eintragen.")
            return
        self.set_busy(True, "Frage SwitchBot-Cloud ab…")
        threading.Thread(target=self._load_devices_worker,
                         args=(token, secret), daemon=True).start()

    def _load_devices_worker(self, token, secret):
        try:
            meters = sb_api.list_meters(token, secret)
            self.msgq.put(("devices", meters))
        except RuntimeError as e:
            self.msgq.put(("error", str(e)))

    def merge_devices(self, meters):
        """Cloud-Liste einarbeiten: Bekanntes behalten, Neues anhaengen."""
        known = {d["id"]: d for d in self.cfg["devices"]}
        found = set()
        for m in meters:
            found.add(m["id"])
            if m["id"] in known:
                known[m["id"]].pop("offline", None)
            else:
                self.cfg["devices"].append(
                    {"id": m["id"], "name": m["name"], "outdoor": False, "selected": False})
        for d in self.cfg["devices"]:
            if d["id"] not in found:
                d["offline"] = True
        self.render_devices()

    # ---------- Speichern / Upload ----------
    def collect(self):
        self.sync_devices()
        try:
            float(self.v_lat.get()), float(self.v_lon.get())
        except ValueError:
            raise ValueError("Breite/Länge müssen Zahlen sein (Punkt als Dezimaltrenner).")
        mode = self.v_mode.get()
        sel = sum(1 for d in self.cfg["devices"] if d["selected"])
        if sel == 0:
            raise ValueError("Mindestens ein Thermometer auswählen.")
        if sel > cs.device_limit(mode):
            raise ValueError(f"Maximal {cs.device_limit(mode)} Thermometer in diesem Modus.")
        self.cfg.update(
            header_title=self.v_title.get().strip() or "SwitchBot Wetter",
            display_mode=mode,
            wifi_ssid=self.v_ssid.get().strip(), wifi_pass=self.v_pass.get(),
            sb_token=self.v_token.get().strip(), sb_secret=self.v_secret.get().strip(),
            lat=self.v_lat.get().strip(), lon=self.v_lon.get().strip(),
            tz=self.v_tz.get().strip())
        return self.cfg

    def on_save(self):
        try:
            cs.save(self.collect())
        except ValueError as e:
            messagebox.showerror("Ungültig", str(e))
            return False
        self.status.config(text="Gespeichert (JSON + Header generiert).")
        return True

    def on_upload(self):
        if self.busy or not self.on_save():
            return
        port = flasher.find_port()
        if not port:
            messagebox.showerror("Kein Board", flasher.PORT_HELP)
            return
        self.set_busy(True, f"Flashe über {port}…")
        self.log_clear()
        threading.Thread(target=self._upload_worker, args=(port,), daemon=True).start()

    def _upload_worker(self, port):
        ok, out = flasher.check_connection(port)
        if not ok:
            self.msgq.put(("log", out))
            self.msgq.put(("error", "Chip antwortet nicht.\n\n" + flasher.PORT_HELP))
            return
        ok = flasher.upload(port, lambda line: self.msgq.put(("log", line)))
        if ok:
            self.msgq.put(("done", "Upload erfolgreich!\n\nDas Board startet nach dem "
                                   "Flashen NICHT von selbst: PWR-Taste aus- und wieder "
                                   "einschalten, dann läuft die neue Firmware."))
        else:
            self.msgq.put(("error", "Upload fehlgeschlagen - Details im Log."))

    # ---------- Infrastruktur ----------
    def set_busy(self, busy, text=""):
        self.busy = busy
        state = "disabled" if busy else "normal"
        self.btn_save.config(state=state)
        self.btn_upload.config(state=state)
        self.status.config(text=text)

    def log_clear(self):
        self.log.config(state="normal")
        self.log.delete("1.0", "end")
        self.log.config(state="disabled")

    def log_line(self, line):
        self.log.config(state="normal")
        self.log.insert("end", line + "\n")
        self.log.see("end")
        self.log.config(state="disabled")

    def poll_queue(self):
        try:
            while True:
                kind, payload = self.msgq.get_nowait()
                if kind == "log":
                    self.log_line(payload)
                elif kind == "devices":
                    self.merge_devices(payload)
                    self.set_busy(False, f"{len(payload)} Sensoren gefunden.")
                elif kind == "done":
                    self.set_busy(False, "Fertig.")
                    messagebox.showinfo("Fertig", payload)
                elif kind == "error":
                    self.set_busy(False, "Fehler.")
                    messagebox.showerror("Fehler", payload)
        except queue.Empty:
            pass
        self.root.after(100, self.poll_queue)


if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()
```

- [ ] **Step 2: Regressionscheck der Modul-Tests**

Run: `python -m unittest discover -s tools -p "test_*.py" -v`
Expected: alle PASS

- [ ] **Step 3: Manueller GUI-Durchlauf (ohne Flash)**

Run: `python tools/configurator.py`
Prüfen:
1. Felder sind mit den echten Werten aus `user_config.h`/`secrets.h` bzw. JSON vorbelegt.
2. „Sensoren laden" holt die 4 bekannten Thermometer; bestehende Namen bleiben erhalten.
3. Modus „6 Thermometer" erlaubt 6 Haken, die anderen Modi nur 4 (Warnung beim 5.).
4. „Speichern" schreibt `tools/wetter_config.json` und regeneriert beide Header; danach `git status` — `include/user_config.h`/`secrets.h`/JSON tauchen NICHT als neue getrackte Dateien auf.
5. Nach dem Speichern: `python -m platformio run -e photopainter` → `SUCCESS` (generierte Header sind gültig).

- [ ] **Step 4: Commit**

```powershell
git add tools/configurator.py
git commit -m @'
feat: tkinter-Konfigurator - Formular, Sensor-Auswahl, Speichern + Upload

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```

---

### Task 9: End-to-End: Flashen + Sichtprüfung + Doku

**Files:**
- Modify: `README.md` (Abschnitt „Konfiguration & Flashen")
- Test: echtes Board (PhotoPainter an USB, Port dynamisch)

- [ ] **Step 1: Upload über das Tool**

`python tools/configurator.py` → Modus „Stunden-Vorschau" wählen (damit das neue Feature sichtbar ist) → „Upload". 
Expected: Port wird automatisch gefunden (z. B. COM4), Log zeigt PlatformIO-Ausgabe, Erfolgsmeldung mit PWR-Hinweis erscheint.

- [ ] **Step 2: Sichtprüfung am Display**

Board per PWR-Taste neu starten, Display-Refresh abwarten (~1 min nach Boot + WLAN). Prüfen:
1. Überschrift wie konfiguriert, Umlaute korrekt (Punkte über „Büro"/„Küche" in den Kacheln).
2. Stundenleiste: 8 Spalten, Uhrzeit/Icon/Temperatur/mm-Wert in Blau.
3. Danach im Tool Modus „6 Thermometer" testen (mit 4 gewählten Sensoren erlaubt): 2×3-Raster ohne Leiste, keine überlaufenden Texte. Anschließend Wunsch-Modus des Nutzers flashen.

Bei Layout-Problemen (Überlappungen, abgeschnittene Texte): Koordinaten in `drawHourlyBar`/`drawTile` nachjustieren, neu flashen, erneut prüfen — Änderungen committen.

- [ ] **Step 3: README ergänzen**

Im README einen Abschnitt „Konfiguration & Flashen" ergänzen (nach dem bestehenden Setup-Teil):

```markdown
## Konfiguration & Flashen

`python tools/configurator.py` startet das Konfigurations-Tool:

- **Sensoren laden** holt alle Thermo-/Hygrometer aus der SwitchBot-Cloud
  (Token/Secret erforderlich). Auswahl, Name, Innen/Außen und Reihenfolge
  sind editierbar.
- **Anzeige-Modus:** Tages-Vorschau (4 Thermometer + 7-Tage-Leiste),
  Stunden-Vorschau (4 Thermometer + 8-Stunden-Leiste) oder 6 Thermometer (2×3).
- **Speichern** schreibt `tools/wetter_config.json` und generiert
  `include/user_config.h` + `include/secrets.h` (alle gitignored).
- **Upload** sucht den PhotoPainter automatisch (USB-VID 303A), prüft die
  Verbindung und flasht per PlatformIO. Nach dem Flashen das Board per
  PWR-Taste aus- und einschalten.
```

- [ ] **Step 4: Commit**

```powershell
git add README.md
git commit -m @'
docs: README - Konfigurator-Tool und Anzeige-Modi dokumentiert

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
'@
```
