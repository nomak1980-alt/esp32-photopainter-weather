# Design: Konfigurations- & Installationstool + Anzeige-Modi

**Datum:** 2026-07-04
**Status:** vom Nutzer genehmigt (Gespräch vom 2026-07-04)

## Ziel

Ein Desktop-Tool (Python/tkinter), mit dem der ESP32-S3-PhotoPainter komplett
eingerichtet werden kann: Sensoren auswählen, Anzeige-Modus und Überschrift
festlegen, Zugangsdaten pflegen — und die Firmware direkt flashen.
Dazu zwei neue Anzeige-Modi in der Firmware (Stunden-Vorschau, 6-Thermometer-Layout).

## Entscheidungen (aus dem Brainstorming)

| Frage | Entscheidung |
|---|---|
| Sensorquelle | Automatisch von der SwitchBot-Cloud (API v1.1, Geräteliste) |
| Tool-Form | Desktop-Fenster, Python/tkinter (keine Zusatz-Abhängigkeiten) |
| Layout-Varianten | Genau 3 Modi: 2×2+Tagesleiste, 2×2+Stundenleiste, 2×3 ohne Leiste |
| Stundenleiste | Nächste 8 Stunden, 1-h-Raster: Uhrzeit, Icon, Temperatur, Niederschlag in mm |
| Umfang | Volles Installationsformular: auch WLAN, SwitchBot-Token/Secret, Standort |
| Konfig-Mechanik | Compile-Time: Tool generiert Header und ruft PlatformIO-Upload auf |

## Architektur

```
tools/configurator.py (tkinter)
   │  liest/schreibt
   ▼
tools/wetter_config.json (gitignored, Quelle der Wahrheit des Tools)
   │  generiert beim Speichern
   ▼
include/user_config.h + include/secrets.h (gitignored, wie bisher)
   │  Upload-Button
   ▼
python -m platformio run -e photopainter -t upload --upload-port COMx
```

## Komponente 1: Konfigurator-Tool (`tools/configurator.py`)

**Abhängigkeiten:** nur Python-Standardbibliothek (tkinter, urllib, hmac, json,
serial-Port-Erkennung über PlatformIO/`list_ports` falls verfügbar, sonst
Registry/WMI-Abfrage). PlatformIO wird als `python -m platformio` aufgerufen
(nicht `pio`, das ist nicht im PATH).

**Formularfelder:**
- Überschrift (Freitext, Standard „SwitchBot Wetter"; Umlaute erlaubt —
  Firmware rendert sie über `printUtf8`)
- Anzeige-Modus (Radiobuttons): Tages-Vorschau / Stunden-Vorschau / 6 Thermometer
- WLAN: SSID, Passwort
- SwitchBot: Token, Secret
- Standort: Breitengrad, Längengrad, Zeitzone (für Open-Meteo)

**Sensorliste:**
- Button „Sensoren laden": GET `https://api.switch-bot.com/v1.1/devices` mit
  HMAC-SHA256-Signatur (Token/Secret/Timestamp/Nonce, wie `sb_sign.cpp`,
  in Python nachgebaut). Gefiltert auf Thermo-/Hygrometer-Typen
  (deviceType enthält "Meter"; inkl. Outdoor-Meter).
- Pro Gerät: Checkbox (Auswahl), Anzeigename (editierbar, vorbelegt mit
  SwitchBot-Namen), Innen/Außen-Haken, Reihenfolge per Hoch/Runter-Buttons.
- Auswahl-Limit je Modus: 4 (Modi mit Wetterleiste) bzw. 6 (Thermometer-Modus).
  Das Limit wird live geprüft; Modus-Wechsel validiert neu.
- Bereits konfigurierte Geräte, die die API-Abfrage nicht liefert, bleiben in
  der Liste erhalten (als „offline/unbekannt" markiert).

**Persistenz:**
- „Speichern" schreibt `tools/wetter_config.json` UND generiert daraus
  `include/user_config.h` + `include/secrets.h` (UTF-8).
- Erster Start ohne JSON: vorhandene `user_config.h`/`secrets.h` werden mit
  einfachem Parsing als Vorbelegung eingelesen (Fallback: Beispielwerte).
- `tools/wetter_config.json` kommt in `.gitignore`.

**Upload-Prüfkette (Button „Upload = Speichern + Flashen"):**
1. Speichern (wie oben).
2. Port-Scan: COM-Port mit USB-VID 303A suchen (Port wechselt, z. B. COM3/COM4).
   Kein Treffer → Dialog mit Anleitung: USB-Datenkabel prüfen, PWR-Taste
   (Board läuft auf Akku weiter!), notfalls Download-Modus (BOOT halten
   während PWR-Power-Cycle, BOOT ~5 s halten).
3. Verbindungstest: esptool `read_mac`/Chip-Erkennung gegen den Port.
4. `python -m platformio run -e photopainter -t upload --upload-port COMx`,
   Ausgabe live in ein Log-Textfeld gestreamt.
5. Ergebnis: Erfolgsmeldung inkl. Hinweis „PWR aus/an" (nativer USB macht
   nach dem Flashen keinen Auto-Reset), oder Fehlermeldung mit Log.

**Fehlerbehandlung:** Netzwerk-/API-Fehler (falsches Token, kein Internet) als
verständliche Messageboxen; Upload-Fehler mit vollständigem Log sichtbar;
Validierung (Koordinaten numerisch, Limit-Überschreitung) vor dem Speichern.

## Komponente 2: Firmware-Erweiterungen

**`user_config.h` (vom Tool generiert, Beispiel aktualisieren):**
- `#define DISPLAY_MODE 0|1|2` (0=Tagesleiste, 1=Stundenleiste, 2=nur Thermometer)
- `#define HEADER_TITLE "…"` (UTF-8)
- `DEVICE_COUNT` 1–4 (Modus 0/1) bzw. 1–6 (Modus 2); Arrays wie bisher.

**Stunden-Vorschau (`forecast.{h,cpp}`):**
- Neu: `HourForecast { hour, wmoCode, temp, precipMm, valid }`,
  `parseHourlyJson()`, `fetchHourlyForecast()` mit
  `hourly=temperature_2m,weather_code,precipitation&forecast_hours=…`
  (die nächsten 8 Stunden ab jetzt; Open-Meteo liefert ab Tagesbeginn,
  daher nach aktueller Stunde filtern — `timezone` wie gehabt).
- Icon-Mapping `wmoToIcon()` wird wiederverwendet.

**Anzeige (`display_view.cpp`):**
- Header-Titel: `printUtf8(HEADER_TITLE, …)` statt fixem String.
- Modus 0: unverändert (2×2, `drawForecastBar`).
- Modus 1: 2×2 + neue `drawHourlyBar()` — 8 Spalten mit Uhrzeit („14:00"),
  Icon, Temperatur mit Gradring, Niederschlag in Blau: „x.x mm"
  (eine Nachkommastelle, bei 0 mm nur „0 mm").
- Modus 2: keine Leiste, Kachelraster 2 Spalten × 3 Reihen über die volle Höhe
  (Kachel ~130 px hoch, aktuelle Schriftgrößen passen).
- Umschaltung per `#if DISPLAY_MODE == …` / `if constexpr`-artigen Konstanten —
  toter Code darf wegoptimiert werden.

**`main.cpp`:** je nach Modus `fetchForecast` oder `fetchHourlyForecast`
aufrufen (Modus 2: gar keinen Forecast laden — spart Zeit/Akku).

## Nicht-Ziele (YAGNI)

- Kein Runtime-Konfigurationssystem (NVS) — Compile-Time reicht.
- Keine weiteren Layout-Varianten als die drei genannten.
- Keine Installer-/Exe-Verpackung des Tools; Start per `python configurator.py`.

## Teststrategie

- `parseHourlyJson` host-seitig testbar halten (wie `parseForecastJson`,
  kein Arduino-Include im Parser-Teil).
- Builds aller drei Modi (`DISPLAY_MODE` durchschalten) müssen kompilieren.
- Tool: Durchlauf gegen echte SwitchBot-API mit vorhandenem Token;
  Header-Generierung per Diff gegen bestehende `user_config.h` prüfen.
- End-to-End: Flash auf den angeschlossenen PhotoPainter (COM-Port dynamisch),
  Sichtprüfung aller drei Modi am Display.
