# Icons v2 (Dithering), echte Fonts, Kachel-Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Neues 9er-Icon-Set (gedithert statt hart quantisiert, inkl. Mond/Nebel + Nachtlogik über `is_day`), generierte 36/32pt-Fonts für Temperatur/Feuchte, Kachel-Redesign (Akku statt „ON", größere Werte), mehr Platz für die Wetterleiste.

**Architecture:** Zwei Generator-Skripte (PIL) erzeugen `src/weather_icons.h` (aus `icons_src/wettericons.png`, Floyd-Steinberg auf die 6 Panelfarben) und `src/big_fonts.h` (Adafruit-GFX-Fonts aus Arial Bold, nur Ziffern+`.-%C`). Firmware: `HourForecast` bekommt `isDay`, Icon-Mapping bekommt Tag/Nacht-Variante + Nebel, `drawTile`/Leisten werden umgebaut.

**Tech Stack:** Python 3 + Pillow (Generatoren, laufen auf dem PC), C++ Arduino/GxEPD2 (Firmware), Unity-Tests `[env:native]`.

**Vom Nutzer entschiedene Anforderungen (Chat 2026-07-04):**
- Icons mit **Dithering** quantisieren (Panel = 6-Farben-E6, Fotos gehen nur per Dithering). Vorschau-PNG zur Abnahme.
- 9 Icons aus `icons_src/wettericons.png`: sun, partly, cloud, rain, snow, storm, moon, moon_partly, fog (untere Vorschauzeile im Blatt ignorieren).
- Stundenleiste: nachts (`is_day=0`) bei WMO 0–2 Mond-Varianten; WMO 45/48 → Nebel-Icon (Tag+Nacht, auch Tagesleiste).
- Kacheln: „ON" ersatzlos weg; rechts oben **Batterie-Piktogramm + Prozent** (rot <20 %, nichts wenn unbekannt/<0); „Batt x%"-Zeile unten entfällt.
- Temperatur **36 pt**, Feuchte **32 pt** (≈10 % kleiner), beide als echte generierte Fonts (Arial Bold, `C:\Windows\Fonts\arialbd.ttf`).
- Wetterleiste (Modus 0/1) wächst von 96 px auf **136 px** (Trennlinie y=384 → **344**).
- Modus 2: 6 exakt gleich große Kacheln (2×3) — bleibt, gleiches neues Kachel-Innenleben.

## Global Constraints

- PlatformIO NUR als `python -m platformio` (pio nicht im PATH).
- Native-Tests: WinLibs-GCC im selben PowerShell-Aufruf in den PATH: `$env:Path = "C:\Users\rsche\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin;$env:Path"` dann `python -m platformio test -e native`.
- Git-Commits mit zwei `-m`-Flags (Titel / `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`) — KEINE PowerShell-Here-Strings.
- Die lokale `include/user_config.h` ist gitignored (steht auf `DISPLAY_MODE 1`) — nie committen; für Modus-Builds Wert temporär ändern, danach zurück auf 1.
- Panel-Palette (Index→Farbe): 0=BLACK 1=WHITE(transparent) 2=RED 3=YELLOW 4=BLUE 5=GREEN; RGB wie bisher in `tools/gen_icons.py` (`PAL`-Liste).
- Kommentar-/Code-Stil wie Bestand: deutsch, knapp.

---

### Task 1: Icon-Generator v2 — Zuschnitt aus dem Blatt + Floyd-Steinberg

**Files:**
- Modify: `tools/gen_icons.py` (kompletter Neuschrieb, alte Version wird ersetzt)
- Regenerate: `src/weather_icons.h`
- Quelle: `icons_src/wettericons.png` (1536×1024, 3×3-Raster mit Beschriftungen, unten Vorschauzeile)

**Interfaces:**
- Produces: `src/weather_icons.h` mit `WICON_W`/`WICON_H` (=64), `WICON_DATA[9]` in GENAU dieser Reihenfolge: sun, partly, cloud, rain, snow, storm, moon, moon_partly, fog (= künftige WIcon-Enum-Reihenfolge, Task 3). Format wie bisher: 4 Bit/Pixel Palettenindex, 2 px/Byte, high-nibble zuerst.
- Produces: `icons_src/_preview_v2.png` (alle 9 Icons nebeneinander auf hellgrauem Grund, 4× vergrößert) zur Abnahme.

- [ ] **Step 1: `tools/gen_icons.py` neu schreiben**

```python
#!/usr/bin/env python
# Schneidet die 9 Wetter-Icons aus icons_src/wettericons.png (3x3-Raster mit
# Beschriftungstext unter jedem Icon, unten eine zu ignorierende Vorschauzeile),
# skaliert auf 64x64 und quantisiert mit Floyd-Steinberg-Dithering auf die
# 6 Panelfarben. Ausgabe: src/weather_icons.h (4 Bit/px) + Vorschau-PNG.
from PIL import Image
import os

SRC   = "C:/Entwicklung/ESP32Wetter/icons_src/wettericons.png"
OUT_H = "C:/Entwicklung/ESP32Wetter/src/weather_icons.h"
PREVIEW = "C:/Entwicklung/ESP32Wetter/icons_src/_preview_v2.png"
NAMES = ["sun","partly","cloud","rain","snow","storm","moon","moon_partly","fog"]
SIZE = 64
WHITE_THR = 242          # heller als das = Hintergrund

# Panel-Palette (Index 0..5): 0=BLACK 1=WHITE 2=RED 3=YELLOW 4=BLUE 5=GREEN
PAL = [(0,0,0),(255,255,255),(190,45,45),(235,190,40),(45,75,180),(50,140,60)]
palimg = Image.new("P", (1,1))
flat = []
for c in PAL: flat += list(c)
palimg.putpalette(flat + flat[:3]*(256-len(PAL)))

def cell_content_box(cell):
    """Icon-Bereich in einer Rasterzelle finden: nicht-weisse Zeilensegmente,
    durch >=12 weisse Zeilen getrennt; das hoechste Segment ist das Icon
    (der Beschriftungstext darunter ist deutlich flacher)."""
    g = cell.convert("L")
    w, h = g.size
    px = g.load()
    rows = [min(px[x, y] for x in range(0, w, 2)) < WHITE_THR for y in range(h)]
    segs, y = [], 0
    while y < h:
        if rows[y]:
            y0 = y
            gap = 0
            while y < h and gap < 12:
                gap = gap + 1 if not rows[y] else 0
                y += 1
            segs.append((y0, y - gap))
        else:
            y += 1
    if not segs: return None
    y0, y1 = max(segs, key=lambda s: s[1] - s[0])
    band = cell.crop((0, y0, w, y1))
    bbox = Image.eval(band.convert("L"), lambda v: 255 if v < WHITE_THR else 0).getbbox()
    if not bbox: return None
    return (bbox[0], y0 + bbox[1], bbox[2], y0 + bbox[3])

def quantize(img):
    """RGB -> Palettenindizes 0..5 mit Floyd-Steinberg; Hintergrund bleibt Weiss(1)."""
    rgb = img.convert("RGB")
    q = rgb.quantize(palette=palimg, dither=Image.FLOYDSTEINBERG)
    return q

im = Image.open(SRC).convert("RGB")
W, H = im.size
grid_h = int(H * 0.78)               # untere Vorschauzeile abschneiden
icons = []
for r in range(3):
    for c in range(3):
        cell = im.crop((c*W//3, r*grid_h//3, (c+1)*W//3, (r+1)*grid_h//3))
        box = cell_content_box(cell)
        assert box, f"kein Icon in Zelle {r},{c}"
        ic = cell.crop(box)
        # weisse Raender exakt trimmen, dann proportional auf SIZE einpassen
        s = SIZE / max(ic.size)
        ic = ic.resize((max(1,round(ic.width*s)), max(1,round(ic.height*s))), Image.LANCZOS)
        canvas = Image.new("RGB", (SIZE, SIZE), (255,255,255))
        canvas.paste(ic, ((SIZE-ic.width)//2, (SIZE-ic.height)//2))
        # fast-weisse Pixel VOR dem Dithern hart auf Weiss ziehen (kein Sprenkeln
        # im transparenten Hintergrund)
        p = canvas.load()
        for y in range(SIZE):
            for x in range(SIZE):
                if min(p[x,y]) > WHITE_THR: p[x,y] = (255,255,255)
        icons.append(quantize(canvas))

# ---- Header schreiben ----
lines = ["// Auto-generiert von tools/gen_icons.py aus icons_src/wettericons.png",
         "// 6-Farben-Panelpalette, Floyd-Steinberg-gedithert. NICHT von Hand editieren.",
         "#pragma once",
         f"#define WICON_W {SIZE}", f"#define WICON_H {SIZE}", ""]
for name, q in zip(NAMES, icons):
    data = list(q.getdata())
    b = bytearray()
    for i in range(0, len(data), 2):
        b.append(((data[i] & 0xF) << 4) | (data[i+1] & 0xF))
    hexes = ",".join(f"0x{v:02X}" for v in b)
    lines.append(f"static const unsigned char WICON_{name.upper()}[] = {{{hexes}}};")
lines.append("")
lines.append("static const unsigned char* const WICON_DATA[] = {")
lines.append("  " + ", ".join(f"WICON_{n.upper()}" for n in NAMES))
lines.append("};")
with open(OUT_H, "w", newline="\n") as f:
    f.write("\n".join(lines) + "\n")

# ---- Vorschau ----
prev = Image.new("RGB", (9*(SIZE*4+8)+8, SIZE*4+16), (225,225,220))
for i, q in enumerate(icons):
    rgb = q.convert("RGB").resize((SIZE*4, SIZE*4), Image.NEAREST)
    prev.paste(rgb, (8 + i*(SIZE*4+8), 8))
prev.save(PREVIEW)
print("ok:", OUT_H, PREVIEW)
```

- [ ] **Step 2: Generator laufen lassen und Vorschau prüfen**

Run: `python tools/gen_icons.py`
Expected: `ok: ...weather_icons.h ..._preview_v2.png`; danach `icons_src/_preview_v2.png` ANSEHEN (Read-Tool): 9 Icons in korrekter Reihenfolge (sun→fog), erkennbar, Hintergrund rein weiß (keine Dither-Sprenkel neben den Motiven), Mond/Nebel sauber. Bei Sprenkeln WHITE_THR justieren (240–250) und neu laufen lassen.

- [ ] **Step 3: Build-Check (Firmware nutzt vorerst nur Icons 0–5)**

Run: `python -m platformio run -e photopainter`
Expected: SUCCESS (WICON_DATA hat jetzt 9 Einträge; Indexe 0–5 unverändert belegt).

- [ ] **Step 4: Commit**

```powershell
git add tools/gen_icons.py src/weather_icons.h icons_src/wettericons.png .gitignore
git commit -m "feat: Icon-Set v2 - 9 Icons aus neuem Blatt, Floyd-Steinberg-Dithering" -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

(Falls `.gitignore` unverändert bleibt, weglassen. `icons_src/cut/`-Eintraege in `.gitignore` bleiben, schaden nicht. `_preview_v2.png` ist durch das bestehende `icons_src/_*.png`-Ignore abgedeckt.)

---

### Task 2: Font-Generator — Arial Bold 36/32 pt als GFX-Fonts

**Files:**
- Create: `tools/gen_fonts.py`
- Create (generiert): `src/big_fonts.h`

**Interfaces:**
- Produces: `src/big_fonts.h` mit zwei `GFXfont`-Strukturen: `ArialBold36` (Temperatur) und `ArialBold32` (Feuchte). Zeichenvorrat: `%`(0x25) bis `C`(0x43) — belegt sind nur `% - . 0-9 C`, Rest Leerglyphen (xAdvance 0). Format exakt Adafruit-GFX (`GFXglyph`/`GFXfont`, Bitmap 1bpp MSB-first, fortlaufend).
- Punktgrößen wie Adafruit fontconvert: Pixelhöhe = pt × 141 dpi / 72 → 36 pt ≈ 70 px, 32 pt ≈ 63 px.

- [ ] **Step 1: `tools/gen_fonts.py` schreiben**

```python
#!/usr/bin/env python
# Erzeugt src/big_fonts.h: Adafruit-GFX-Fonts aus Arial Bold (Windows),
# nur die Zeichen % - . 0-9 C (Anzeige von Temperatur/Feuchte).
# Punkt->Pixel wie Adafruit fontconvert: 141 dpi.
from PIL import Image, ImageDraw, ImageFont

TTF = r"C:\Windows\Fonts\arialbd.ttf"
OUT = "C:/Entwicklung/ESP32Wetter/src/big_fonts.h"
PREVIEW = "C:/Entwicklung/ESP32Wetter/icons_src/_fonts_preview.png"
CHARS = "%-.0123456789C"
FIRST, LAST = 0x25, 0x43   # '%'..'C'

def build_font(name, pt):
    px = round(pt * 141 / 72)
    font = ImageFont.truetype(TTF, px)
    ascent, descent = font.getmetrics()
    glyphs = {}   # code -> (bitmap_bytes, w, h, xadv, xoff, yoff)
    for ch in CHARS:
        x0, y0, x1, y1 = font.getbbox(ch)
        w, h = x1 - x0, y1 - y0
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-x0, -y0), ch, fill=255, font=font)
        bits = bytearray()
        acc, nacc = 0, 0
        for v in img.getdata():
            acc = (acc << 1) | (1 if v >= 128 else 0)
            nacc += 1
            if nacc == 8:
                bits.append(acc); acc, nacc = 0, 0
        if nacc: bits.append(acc << (8 - nacc))
        xadv = round(font.getlength(ch))
        glyphs[ord(ch)] = (bytes(bits), w, h, xadv, x0, y0 - ascent)
    # zusammensetzen
    bitmap = bytearray()
    table = []
    for code in range(FIRST, LAST + 1):
        if code in glyphs:
            b, w, h, xadv, xo, yo = glyphs[code]
            table.append((len(bitmap), w, h, xadv, xo, yo))
            bitmap += b
        else:
            table.append((len(bitmap), 0, 0, 0, 0, 0))
    yadv = ascent + descent
    lines = [f"static const uint8_t {name}Bitmaps[] PROGMEM = {{"]
    lines.append("  " + ",".join(f"0x{v:02X}" for v in bitmap))
    lines.append("};")
    lines.append(f"static const GFXglyph {name}Glyphs[] PROGMEM = {{")
    for off, w, h, xadv, xo, yo in table:
        lines.append(f"  {{{off},{w},{h},{xadv},{xo},{yo}}},")
    lines.append("};")
    lines.append(f"static const GFXfont {name} PROGMEM = {{")
    lines.append(f"  (uint8_t*){name}Bitmaps, (GFXglyph*){name}Glyphs, 0x{FIRST:02X}, 0x{LAST:02X}, {yadv}")
    lines.append("};")
    return lines, glyphs, ascent

out = ["// Auto-generiert von tools/gen_fonts.py (Arial Bold, nur %-.0-9C).",
       "// NICHT von Hand editieren.", "#pragma once",
       "#include <Adafruit_GFX.h>", ""]
previews = []
for name, pt in (("ArialBold36", 36), ("ArialBold32", 32)):
    lines, glyphs, ascent = build_font(name, pt)
    out += lines + [""]
    previews.append((name, pt, glyphs, ascent))

with open(OUT, "w", newline="\n") as f:
    f.write("\n".join(out) + "\n")

# Vorschau: "24.5C 38%" aus den generierten Bitmaps rekonstruieren
img = Image.new("L", (1200, 260), 255)
ypos = 20
for name, pt, glyphs, ascent in previews:
    x = 10
    for ch in "24.5C 38%":
        if ch == " ": x += 20; continue
        b, w, h, xadv, xo, yo = glyphs[ord(ch)]
        gl = Image.new("L", (max(w,1), max(h,1)), 255)
        px = gl.load()
        for i in range(w * h):
            if b[i >> 3] & (0x80 >> (i & 7)): px[i % w, i // w] = 0
        img.paste(gl, (x + xo, ypos + ascent + yo))
        x += xadv
    ypos += 120
img.save(PREVIEW)
print("ok:", OUT, PREVIEW)
```

- [ ] **Step 2: Generator laufen lassen, Vorschau ansehen**

Run: `python tools/gen_fonts.py`
Expected: `ok: ...big_fonts.h ..._fonts_preview.png`. Vorschau ANSEHEN: „24.5C 38%" zweimal (36 pt / 32 pt), Glyphen sauber, keine abgeschnittenen Kanten, 32 pt sichtbar ~10 % kleiner.

- [ ] **Step 3: Compile-Check**

Temporär prüfen, dass der Header baut: in `src/display_view.cpp` `#include "big_fonts.h"` ergänzen (bleibt drin — Task 4 nutzt ihn), dann:
Run: `python -m platformio run -e photopainter`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```powershell
git add tools/gen_fonts.py src/big_fonts.h src/display_view.cpp
git commit -m "feat: generierte Arial-Bold-GFX-Fonts 36/32pt fuer Temperatur/Feuchte" -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: Nachtlogik + Nebel im Forecast (TDD, native)

**Files:**
- Modify: `src/forecast.h`, `src/forecast.cpp`
- Test: `test/test_forecast/test_forecast.cpp`, `test/test_forecast/main.cpp`

**Interfaces:**
- Consumes: nichts Neues.
- Produces:
  - `enum WIcon { ICON_SUN, ICON_PARTLY, ICON_CLOUD, ICON_RAIN, ICON_SNOW, ICON_STORM, ICON_MOON, ICON_MOON_PARTLY, ICON_FOG };` (Reihenfolge = WICON_DATA aus Task 1!)
  - `HourForecast` bekommt zusätzlich `bool isDay;`
  - `WIcon wmoToIcon(int code)` — NEU: 45/48 → `ICON_FOG` (statt CLOUD), sonst unverändert.
  - NEU `WIcon wmoToIconDN(int code, bool isDay)` — wie wmoToIcon, aber nachts: Code 0..1 → `ICON_MOON`, Code 2 → `ICON_MOON_PARTLY`.
  - `parseHourlyJson`/`fetchHourlyForecast`: lesen zusätzlich `is_day` (Array aus 0/1); URL-Parameter wird `hourly=temperature_2m,weather_code,precipitation,is_day`. Fehlt `is_day` im JSON, gilt `isDay=true` (rueckwaertskompatibel, kein Fehler).

- [ ] **Step 1: Tests anpassen/ergänzen (failing)**

In `test/test_forecast/test_forecast.cpp`:
- In `test_wmo_to_icon` die Zeile `TEST_ASSERT_EQUAL(ICON_CLOUD, wmoToIcon(45));` ändern zu `TEST_ASSERT_EQUAL(ICON_FOG, wmoToIcon(45));` und ergänzen: `TEST_ASSERT_EQUAL(ICON_FOG, wmoToIcon(48));`
- Neu:

```cpp
void test_wmo_to_icon_night() {
  TEST_ASSERT_EQUAL(ICON_MOON,         wmoToIconDN(0, false));
  TEST_ASSERT_EQUAL(ICON_MOON,         wmoToIconDN(1, false));
  TEST_ASSERT_EQUAL(ICON_MOON_PARTLY,  wmoToIconDN(2, false));
  TEST_ASSERT_EQUAL(ICON_SUN,          wmoToIconDN(0, true));
  TEST_ASSERT_EQUAL(ICON_PARTLY,       wmoToIconDN(2, true));
  TEST_ASSERT_EQUAL(ICON_RAIN,         wmoToIconDN(61, false));  // Regen bleibt Regen
  TEST_ASSERT_EQUAL(ICON_FOG,          wmoToIconDN(45, false));
}

void test_parse_hourly_is_day() {
  const char* body = R"({"hourly":{"time":["2026-07-04T21:00","2026-07-04T22:00"],
    "weather_code":[0,2],"temperature_2m":[19.0,18.2],
    "precipitation":[0.0,0.0],"is_day":[0,0]}})";
  HourForecast h[8];
  int count = 0;
  TEST_ASSERT_TRUE(parseHourlyJson(body, h, 8, &count));
  TEST_ASSERT_FALSE(h[0].isDay);
  TEST_ASSERT_EQUAL(ICON_MOON, wmoToIconDN(h[0].wmoCode, h[0].isDay));
  TEST_ASSERT_EQUAL(ICON_MOON_PARTLY, wmoToIconDN(h[1].wmoCode, h[1].isDay));
}

void test_parse_hourly_is_day_missing_defaults_day() {
  const char* body = R"({"hourly":{"time":["2026-07-04T14:00"],
    "weather_code":[0],"temperature_2m":[27.0],"precipitation":[0.0]}})";
  HourForecast h[8];
  int count = 0;
  TEST_ASSERT_TRUE(parseHourlyJson(body, h, 8, &count));
  TEST_ASSERT_TRUE(h[0].isDay);
}
```

In `test/test_forecast/main.cpp` deklarieren + registrieren (`RUN_TEST` für alle drei neuen).

- [ ] **Step 2: Fehlschlag sehen**

Run (PATH-Prefix laut Global Constraints): `python -m platformio test -e native -f test_forecast`
Expected: Compile-FAIL (`ICON_FOG`/`wmoToIconDN` unbekannt).

- [ ] **Step 3: Implementierung**

`src/forecast.h`: Enum erweitern (Reihenfolge!), `bool isDay;` in `HourForecast`, Deklaration `WIcon wmoToIconDN(int code, bool isDay);`.

`src/forecast.cpp`:

```cpp
WIcon wmoToIcon(int c) {
  if (c == 0) return ICON_SUN;
  if (c == 1 || c == 2) return ICON_PARTLY;
  if (c == 45 || c == 48) return ICON_FOG;
  if (c == 3) return ICON_CLOUD;
  if (c == 71 || c == 73 || c == 75 || c == 77 || c == 85 || c == 86) return ICON_SNOW;
  if (c == 95 || c == 96 || c == 99) return ICON_STORM;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return ICON_RAIN;
  return ICON_CLOUD;
}

// Tag/Nacht-Variante: nachts wird aus Sonne Mond (0..1) bzw. Mond+Wolke (2).
WIcon wmoToIconDN(int c, bool isDay) {
  if (!isDay) {
    if (c == 0 || c == 1) return ICON_MOON;
    if (c == 2) return ICON_MOON_PARTLY;
  }
  return wmoToIcon(c);
}
```

`parseHourlyJson`: nach `JsonArray prec = ...`:

```cpp
  JsonArray isday = hourly["is_day"];   // optional (aeltere Antworten)
```

und in der Schleife:

```cpp
    out[n].isDay = isday.isNull() ? true : (isday[i].as<int>() != 0);
```

`fetchHourlyForecast`: URL-Teil `"&hourly=temperature_2m,weather_code,precipitation"` → `"&hourly=temperature_2m,weather_code,precipitation,is_day"` (Achtung: der String steht als `"...&hourly=..."`-Konkatenation im Code, exakt dort ergänzen).

- [ ] **Step 4: Tests grün sehen**

Run: `python -m platformio test -e native -f test_forecast`
Expected: alle PASS (inkl. bestehender).

- [ ] **Step 5: Commit**

```powershell
git add src/forecast.h src/forecast.cpp test/test_forecast/
git commit -m "feat: Nacht-Icons (is_day) und Nebel-Mapping im Forecast" -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: Display-Umbau — Kacheln, Fonts, Leisten-Geometrie

**Files:**
- Modify: `src/display_view.cpp` (Hauptarbeit), ggf. `src/displaytest.cpp` (nur falls es „ON"/Batt-Annahmen hat — prüfen)

**Interfaces:**
- Consumes: `ArialBold36`/`ArialBold32` aus `src/big_fonts.h` (Task 2), `wmoToIconDN` + `ICON_FOG` (Task 3), `hf[i].isDay`.
- Produces: keine neuen Schnittstellen; `displayRender`-Signatur bleibt.

- [ ] **Step 1: Kachel-Redesign in `drawTile`**

Neuer Aufbau (Kachelhöhe 126–128 px):

```cpp
// Kleines Batterie-Piktogramm: Rahmen + Fuellstand + Nase rechts.
static void drawBattIcon(int x, int y, int pct, uint16_t color) {
  display.drawRect(x, y, 24, 12, color);
  display.fillRect(x + 26, y + 3, 3, 6, color);
  int fill = pct > 0 ? (pct * 20) / 100 : 0;
  if (fill > 0) display.fillRect(x + 2, y + 2, fill, 8, color);
}

static void drawTile(int x, int y, int w, int h, const char* name, const SensorReading& r) {
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  display.setTextSize(1);
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x + 14, y + 30);
  printUtf8(name, GxEPD_BLACK);
  // Akku rechts oben: Piktogramm + Prozent (nichts, wenn unbekannt)
  if (r.valid && r.battery >= 0) {
    uint16_t bc = batteryWarn(r.battery) ? GxEPD_RED : GxEPD_BLACK;
    char bp[8];
    snprintf(bp, sizeof bp, "%d%%", r.battery);
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(bp, 0, 0, &bx, &by, &bw, &bh);
    int tx = x + w - 14 - bw;
    display.setTextColor(bc);
    display.setCursor(tx, y + 30);
    display.print(bp);
    drawBattIcon(tx - 38, y + 12, r.battery, bc);
  }
  char buf[16];
  if (r.valid) {
    // Temperatur gross (36pt), Feuchte daneben (32pt, ~10% kleiner)
    char hum[8];
    fmtTemp(r, buf, sizeof buf);
    display.setFont(&ArialBold36);
    printTempC(x + 18, y + h - 22, buf, 7, 46, toGx(tempColor(r.temperature)));
    int hx = display.getCursorX();
    fmtHum(r, hum, sizeof hum);
    display.setFont(&ArialBold32);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(hx + 34, y + h - 22);
    display.print(hum); display.print("%");
  } else {
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x + 18, y + (h + 10) / 2);
    display.print("-- keine Daten --");
  }
}
```

Hinweise: `printTempC` bleibt unverändert nutzbar (nutzt den aktuell gesetzten Font); `setTextSize(2)`-Aufrufe für Temperatur/Feuchte ENTFALLEN komplett. „ON"/`Batt`-Zeile sind ersatzlos gestrichen. `%` ist im ArialBold32-Zeichenvorrat enthalten.

- [ ] **Step 2: Header-Temperatur auf 36 pt umstellen**

In `displayRender`: den Block für den lokalen SHTC3-Wert („Hier") von `FreeSansBold18pt7b` + `setTextSize(2)` auf `&ArialBold36` + `setTextSize(1)` umstellen (printTempC-Aufruf: circleDY 46 statt 38); die Feuchte daneben von 18 pt auf `&ArialBold32` (Baseline beibehalten, Cursor-X wie gehabt aus `getCursorX()+34`). Restlicher Header (Titel, Untertitel) unverändert.

- [ ] **Step 3: Leisten-Geometrie 136 px**

In `displayRender`:

```cpp
#if DISPLAY_MODE == 2
    const int FC_Y = 480;
    const int ROWS = 3;
#else
    const int FC_Y = 344;
    const int ROWS = 2;
#endif
```

(Kachelraster-Formel bleibt — ergibt Modus 0/1: 2 Reihen à 126 px; Modus 2: 3 Reihen à 128 px, alle gleich groß.)

`drawForecastBar` (Höhe jetzt 136 px, y0=344): Trennlinien `y0+6 .. y0+128`; Wochentag-Baseline `y0+24`; Icon-Zentrum `(cx, y0+68)` (das `cx+16`-Offset entfällt, Icon mittig); Temperatur-Baseline `y0+128`, Gradringe `y0+116`.

`drawHourlyBar` analog: Trennlinien `y0+6 .. y0+128`; Uhrzeit-Baseline `y0+24`; Icon `drawWeatherIcon(cx, y0+68, wmoToIconDN(hf[i].wmoCode, hf[i].isDay));` (NACHTLOGIK!); Temperatur-Baseline `y0+128`, Gradring `y0+116`, mm-Text Baseline `y0+128`.

- [ ] **Step 4: displaytest.cpp prüfen/anpassen**

`src/displaytest.cpp` lesen: Demo-Daten so ergänzen, dass `isDay` gesetzt ist (falls dort HourForecast-Demos existieren — sonst nichts zu tun). Sicherstellen, dass es baut.

- [ ] **Step 5: Builds aller Modi**

`include/user_config.h`: DISPLAY_MODE temporär 0 → bauen, 1 → bauen, 2 → bauen, danach wieder **1** (Nutzer-Modus!).
Run (3×): `python -m platformio run -e photopainter` — je SUCCESS.
Zusätzlich: `python -m platformio run -e displaytest` — SUCCESS.

- [ ] **Step 6: Commit**

```powershell
git add src/display_view.cpp src/displaytest.cpp
git commit -m "feat: Kachel-Redesign (Akku-Icon statt ON, 36/32pt-Werte), Leiste 136px, Nacht-Icons in Stundenleiste" -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: End-to-End — Flash + Sichtprüfung + Doku

**Files:**
- Modify: `README.md` (Icon-/Anzeige-Absatz aktualisieren, falls er „6 Farben quantisiert/ON" erwähnt)
- Test: echtes Board (Nutzer!)

- [ ] **Step 1: Flash** — Board an USB (ggf. Download-Modus: BOOT halten beim PWR-Einschalten). Upload über den Konfigurator oder `python -m platformio run -e photopainter -t upload --upload-port <PORT>` (Port via `tools/flasher.py::find_port`). Damit geht auch der noch ausstehende mm-Fix (4b4c21d) mit aufs Board.
- [ ] **Step 2: Sichtprüfung durch den Nutzer** — Kacheln (Akku-Symbol, große glatte Zahlen, Feuchte ~10 % kleiner), Stundenleiste (Icons gedithert ok? nachts Mond?), keine Überlappungen. Bei Bedarf Koordinaten nachjustieren + committen.
- [ ] **Step 3: README-Absatz aktualisieren + Commit** (`docs: ...`).
