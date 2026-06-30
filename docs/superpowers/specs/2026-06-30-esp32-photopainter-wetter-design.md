# ESP32-S3-PhotoPainter „SwitchBot Wetter" — Design

**Datum:** 2026-06-30
**Status:** Design freigegeben, vor Implementierungsplanung

## Ziel

Die Informationen der bestehenden Windows-App `C:\Entwicklung\SwitchbotWetter`
(SwitchBot-Sensoren: Temperatur, Luftfeuchtigkeit, Batterie, Online-Status,
Zeitstempel) auf einem **Waveshare ESP32-S3-PhotoPainter** (7,3" Spectra-6 /
E6 Vollfarb-E-Paper, 800×480) darstellen — angepasst an das Display.

Zusätzlich wird der **onboard SHTC3**-Temperatur-/Feuchtefühler des PhotoPainters
als lokaler Referenzwert angezeigt.

Der PhotoPainter arbeitet **autark vom PC**.

## Hardware

- **SoC:** ESP32-S3-WROOM-1-**N16R8** (16 MB Flash, 8 MB PSRAM, 240 MHz).
  PlatformIO-Board entsprechend (z. B. `esp32-s3-devkitc-1` mit
  `board_build.flash_size = 16MB`, PSRAM aktiviert).
- **Programmierung/Log:** Type-C (nativer USB, VID 303A → COM-Port, hier COM3).
- **Bedienelemente:** KEY (frei belegbar — v2-Kandidat „manuell aktualisieren"),
  BOOT (Download-Modus), PWR (Ein/Aus).
- **TF-Karte (FAT32):** vorhanden — v2-Kandidat für Logging/Cache. Nicht in v1.
- **Audio (ES7210 ADC, ES8311 DAC, Dual-Mic, Speaker-Header):** für dieses
  Projekt irrelevant.
- **RTC-Backup-Batterie-Header:** hält die PCF85063 auch ohne Hauptakku.

## Onboard-Peripherie des PhotoPainters (laut Hersteller)

- **SHTC3** Temperatur-/Feuchtesensor (I²C) → lokaler Messwert, auch offline
  verfügbar. Anzeige in der Kopfzeile ("Hier: ..").
- **PCF85063 RTC** (I²C) → präzise Hardware-Uhr, hält die Zeit über Deep-Sleep
  hinweg. Dadurch **kein NTP-Sync bei jedem Aufwachen** nötig — nur gelegentlich
  nachsyncen (z. B. 1×/Tag). Die Tag/Nacht-Wakeup-Logik stützt sich auf die RTC.
- **Power-Management-Chip** (I²C) → eigener Akkustand (siehe unten).
  ⚠️ **Offen:** Hersteller-Featureliste nennt **„TG28"**, der Arduino-
  Beispielcode im Repo nutzt aber **AXP2101** (`XPowersLib`). Tatsächlichen Chip
  vor der Implementierung per **I²C-Scan auf der Hardware** verifizieren; danach
  passende Lib/Ausleseroutine wählen.

## Datenquelle: SwitchBot Cloud API (über Hub 3)

Statt selbst per BLE zu scannen (Reichweitenproblem zu den Außensensoren, in der
PC-App dokumentiert), holt der ESP32 die Werte aus der **SwitchBot Cloud**. Der
immer-online **Hub 3** hat alle Sensoren bereits zuverlässig eingebunden.

- **Endpoint:** `GET https://api.switch-bot.com/v1.1/devices/{deviceId}/status`
  pro Sensor → liefert `temperature`, `humidity`, `battery`.
- **Geräteliste / Mapping:** einmalig `GET /v1.1/devices`, um die 4 Geräte den
  `deviceId`s zuzuordnen (i. d. R. MAC ohne Doppelpunkte). Mapping wird fix in
  `config.h` hinterlegt.
- **Auth:** HMAC-SHA256 über `token + t + nonce`, gesendet in den Headern
  `Authorization` (= token), `sign`, `t` (ms-Timestamp), `nonce`. Berechnung auf
  dem ESP32 mit **mbedTLS** (`mbedtls_md_hmac`, SHA256, Base64).
- **TLS:** `WiFiClientSecure` mit `setInsecure()` (kein Cert-Pinning).
- **Rate-Limit:** 10.000 Calls/Tag erlaubt; Bedarf ~576/Tag (4 Sensoren × bis zu
  144 Wakeups). Unkritisch.
- **API-Zugang muss noch eingerichtet werden:** Token + Secret werden in der
  SwitchBot-App erzeugt (Profil → App-Version 10× tippen → Developer Options).

### Sensoren (aus config.json der PC-App)

| Anzeigename   | MAC               | Position |
|---------------|-------------------|----------|
| Außen Hinten  | AA:AA:AA:AA:AA:AA | außen    |
| Außen Vorne   | BB:BB:BB:BB:BB:BB | außen    |
| Büro          | CC:CC:CC:CC:CC:CC | innen    |
| Küche         | DD:DD:DD:DD:DD:DD | innen    |

## Betrieb & Stromsparen

- **Batteriebetrieb**, Deep-Sleep zwischen den Updates.
- **Wakeup-Intervall:**
  - Tag (05:00–24:00): alle **10 min**
  - Nacht (00:00–05:00): alle **30 min**
  - Entscheidung anhand der **PCF85063-RTC**-Uhrzeit beim Aufwachen (RTC wird
    ~1×/Tag per NTP nachgezogen, nicht bei jedem Wakeup).
- **Refresh nur bei Änderung:** Nach dem Datenabruf werden die Werte mit dem
  letzten Stand im **RTC-RAM** verglichen (`RTC_DATA_ATTR`). Vollbild-Refresh des
  E-Papers nur, wenn sich Temperatur, Feuchte oder eine Batterie geändert hat.
  Sonst sofort zurück in Deep-Sleep.
- **Kein Teil-Refresh möglich:** Das E6-Farbpanel kann technisch nur Vollbild
  refreshen (~15–30 s, sichtbares Farbflackern). Daher keine Live-Uhr; der
  „Stand"-Zeitstempel zeigt nur den letzten Abrufzeitpunkt.

## Display-Layout (2×2 mit Farb-Akzenten, 800×480 quer)

```
+------------------------------------------------------------+
| SwitchBot Wetter   Hier: 22.8°C 50%   14:30   [Akku 84% ⚡]|
+----------------------------+-------------------------------+
| Außen Hinten        ● ON   | Außen Vorne          ● ON     |
|     21.4 °C  (grün)        |     19.8 °C  (grün)           |
|     65 %                   |     72 %                      |
|   Batt 88%   14:30         |   Batt 91%   14:29            |
+----------------------------+-------------------------------+
| Büro                ● ON   | Küche                ● —      |
|     23.1 °C  (grün)        |     — keine Daten —           |
|     48 %                   |                               |
|   Batt 76%   14:30         |                               |
+----------------------------+-------------------------------+
```

- **Header:** Titel links; mittig der **lokale SHTC3-Wert** („Hier: 22.8 °C
  50 %"); rechts „HH:MM" (letzter Abruf / RTC) + **PhotoPainter-Akku** in % mit
  Lade-Symbol (⚡) wenn am Laden.
- **Anordnung:** Außensensoren oben, Innensensoren unten.
- **Pro Kachel:** Name, großer Temperaturwert, Feuchte, „Batt xx%" + Sensor-
  Zeitstempel, Online-Punkt (grün = ON / grau = keine Daten).
- **Farbcodierung Temperatur:** < 10 °C blau, 10–25 °C grün, > 25 °C rot.
- **Batterie-Warnung:** Sensor-Batterie < 20 % → Zahl rot.
- **Keine Daten:** „— keine Daten —" grau.

## PhotoPainter-Akkustand & lokaler Sensor

**Akku (PMIC, I²C)** — vermutlich **AXP2101** (per `XPowersLib`), Chip aber per
I²C-Scan zu bestätigen (siehe Onboard-Peripherie). Bei AXP2101:

- `getBatteryPercent()` → Akku %
- `getBattVoltage()` → mV
- `isCharging()` → Lade-Symbol im Header

**Lokaler Sensor (SHTC3, I²C)** — Temperatur + Luftfeuchtigkeit am Standort des
Rahmens; wird bei jedem Wakeup gelesen (kein Funk nötig) und im Header angezeigt.

**Uhrzeit (PCF85063 RTC, I²C)** — liefert die aktuelle Zeit für „Stand"-Anzeige
und Tag/Nacht-Intervall; ~1×/Tag per NTP nachgezogen.

## Architektur / Datenfluss

```
[SwitchBot Sensoren] → [Hub 3] → SwitchBot Cloud (api.switch-bot.com)
                                          │ HTTPS/JSON
alle 10/30 min Wakeup:                    ▼
   [ESP32-S3 PhotoPainter] ── WLAN ──► SwitchBot-Werte holen
            │
            ├─ SHTC3 (I²C): lokale Temp/Feuchte lesen
            ├─ AXP2101 (I²C): eigenen Akku lesen
            ├─ PCF85063 (I²C): Uhrzeit / Tag-Nacht-Intervall
            ├─ Vergleich mit RTC-RAM (letzter Stand)
            │
            ├─ geändert? ──ja──► Vollbild-Refresh E-Paper ──► Deep-Sleep
            └────────────  nein ────────────────────────────► Deep-Sleep
```

## Komponenten (PlatformIO, Arduino-Framework, ESP32-S3)

| Modul            | Aufgabe                                                                 | Abhängigkeit                       |
|------------------|-------------------------------------------------------------------------|------------------------------------|
| `config.h`       | WLAN SSID/Passwort, API token+secret, Sensor-Mapping, Intervalle        | —                                  |
| `switchbot_api`  | HTTPS-Call, HMAC-SHA256-Signatur, JSON → `SensorReading[]`              | WiFiClientSecure, ArduinoJson, mbedTLS |
| `local_sensors`  | SHTC3 (Temp/Feuchte) + AXP2101 (Akku) + PCF85063 (Uhrzeit) auslesen      | SHTC3-Lib, XPowersLib, RTC-Lib (I²C) |
| `display_view`   | 2×2-Farb-Layout + Header auf den E-Paper-Framebuffer zeichnen           | Waveshare E6-Treiber (aus Repo)    |
| `power`          | Deep-Sleep, Wakeup-Intervall nach RTC-Uhrzeit, RTC-RAM-Vergleich        | esp_sleep                          |
| `main.ino`       | Ablauf: wake → SHTC3/Akku/RTC → WLAN → API → vergleichen → ggf. zeichnen → sleep | alle obigen                |

### Datenstruktur (analog zur PC-App `SensorReading`)

```cpp
struct SensorReading {
  float   temperature;   // °C
  int     humidity;      // %
  int     battery;       // % (Sensor), -1 = unbekannt
  bool    valid;         // Daten vorhanden?
  // Zeitstempel: gemeinsamer "Stand"-Zeitpunkt des Abrufs (NTP)
};
```

### Display-Treiber

Der E6-Panel-Treiber wird aus dem offiziellen Repo
`waveshareteam/ESP32-S3-PhotoPainter` übernommen (Repo wird ins Projekt geklont;
EPD- und I²C-Pinbelegung daraus, nicht raten).

## Fehlerbehandlung

- **WLAN-Verbindung scheitert:** letzten Stand behalten, kein Refresh, kleines
  Warn-Symbol im Header, wieder schlafen.
- **API-Fehler / einzelner Sensor fehlt:** betroffenen Sensor als „keine Daten"
  bzw. letzten bekannten Wert anzeigen; übrige normal.
- **NTP scheitert:** Intervall-Entscheidung fällt auf Tag-Default (10 min)
  zurück; „Stand"-Zeit ggf. ohne Aktualisierung.
- **Erststart ohne Daten:** „Verbinde…" bzw. „— keine Daten —".

## Testing

- **`switchbot_api`:** HMAC-Signatur + JSON-Parsing als **PlatformIO native
  Host-Unit-Tests** mit gespeicherten Beispiel-Responses (bekannte
  Signatur-Vektoren, Beispiel-Status-JSON).
- **`display_view`:** gegen einen Mock-Framebuffer — korrekte Texte/Farben je
  Wertebereich (Temp-Farbe, Batterie-Warnung, „keine Daten"), ohne Hardware.
- **`power`:** Intervall-Logik (Tag/Nacht) und Änderungserkennung als reine
  Funktionen testbar.
- **End-to-End:** manueller Flash + Beobachtung auf COM3 (USB native, VID 303A).

## Bewusst NICHT in v1 (YAGNI)

- Konfiguration der WLAN-/API-Zugänge zur Laufzeit (Captive Portal) — fix in
  `config.h`.
- Web-UI / OTA-Updates.
- Historie / Graphen.
- Direkter BLE-Scan auf dem ESP32 (verworfen wegen Reichweite; Cloud gewählt).
```
