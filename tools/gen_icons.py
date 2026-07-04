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
    durch >=10 weisse Zeilen getrennt. Zeilen der Nachbarzelle darueber koennen
    als duenner Streifen am oberen Zellenrand einbluten (y0 nahe 0) und werden
    verworfen. Von den verbleibenden Segmenten ist das letzte der Beschriftungs-
    text darunter; alle davor gehoeren zum Icon (das oft selbst in mehrere
    Segmente zerfaellt, z.B. Regentropfen/Schneeflocken/Nebelbalken). Bei storm.png
    betraegt der Abstand Icon->Text nur 10 Zeilen (enger als bei den anderen
    Zellen) -- der Schwellwert muss <=10 sein, damit er dort noch trennt."""
    g = cell.convert("L")
    w, h = g.size
    px = g.load()
    rows = [min(px[x, y] for x in range(0, w, 2)) < WHITE_THR for y in range(h)]
    segs, y = [], 0
    while y < h:
        if rows[y]:
            y0 = y
            gap = 0
            while y < h and gap < 10:
                gap = gap + 1 if not rows[y] else 0
                y += 1
            segs.append((y0, y - gap))
        else:
            y += 1
    segs = [s for s in segs if s[0] >= 3]   # obere Randeinbluten verwerfen
    if not segs: return None
    icon_segs = segs[:-1] if len(segs) > 1 else segs
    y0 = min(s[0] for s in icon_segs)
    y1 = max(s[1] for s in icon_segs)
    band = cell.crop((0, y0, w, y1))
    bbox = Image.eval(band.convert("L"), lambda v: 255 if v < WHITE_THR else 0).getbbox()
    if not bbox: return None
    return (bbox[0], y0 + bbox[1], bbox[2], y0 + bbox[3])

def quantize(img):
    """RGB -> Palettenindizes 0..5 mit Floyd-Steinberg; Hintergrund bleibt Weiss(1)."""
    rgb = img.convert("RGB")
    q = rgb.quantize(palette=palimg, dither=Image.FLOYDSTEINBERG)
    return q

def band_columns(band):
    """Spaltensegmente eines Zeilenbands finden (>=40 weisse Spalten Abstand).
    Ein starres W//3-Raster geht NICHT: die Icons ragen teils ueber die
    Drittelgrenzen (cloud/storm wuerden links beschnitten, partly/snow/
    moon_partly bekaemen den Anschnitt des Nachbar-Icons in die Box und
    wuerden dadurch beim Einpassen geschrumpft)."""
    g = band.convert("L")
    w, h = g.size
    px = g.load()
    cols = [min(px[x, y] for y in range(0, h, 2)) < WHITE_THR for x in range(w)]
    segs, x = [], 0
    while x < w:
        if cols[x]:
            x0 = x
            gap = 0
            while x < w and gap < 40:
                gap = gap + 1 if not cols[x] else 0
                x += 1
            segs.append((x0, x - gap))
        else:
            x += 1
    return segs

im = Image.open(SRC).convert("RGB")
W, H = im.size
grid_h = int(H * 0.78)               # untere Vorschauzeile abschneiden
icons = []
for r in range(3):
    y0b, y1b = r*grid_h//3, (r+1)*grid_h//3
    band = im.crop((0, y0b, W, y1b))
    csegs = band_columns(band)
    assert len(csegs) == 3, f"Zeile {r}: erwarte 3 Spaltensegmente, habe {csegs}"
    for c in range(3):
        x0, x1 = csegs[c]
        cell = band.crop((x0, 0, x1, y1b - y0b))
        box = cell_content_box(cell)
        assert box, f"kein Icon in Zelle {r},{c}"
        ic = cell.crop(box)
        # weisse Raender exakt trimmen (beide Achsen!), dann proportional auf
        # SIZE einpassen -- Leerraum in der Box wuerde das Motiv schrumpfen
        tb = Image.eval(ic.convert("L"), lambda v: 255 if v < WHITE_THR else 0).getbbox()
        if tb: ic = ic.crop(tb)
        s = SIZE / max(ic.size)
        ic = ic.resize((max(1,round(ic.width*s)), max(1,round(ic.height*s))), Image.LANCZOS)
        canvas = Image.new("RGB", (SIZE, SIZE), (255,255,255))
        canvas.paste(ic, ((SIZE-ic.width)//2, (SIZE-ic.height)//2))
        # VOR dem Dithern: fast-weisse Pixel hart auf Weiss (kein Sprenkeln im
        # Hintergrund); dunkle UND ungesaettigte Pixel hart auf Schwarz, damit
        # die Navy-Konturen keine roten/gruenen Dither-Fransen bekommen
        p = canvas.load()
        for y in range(SIZE):
            for x in range(SIZE):
                v = p[x,y]
                if min(v) > WHITE_THR:
                    p[x,y] = (255,255,255)
                elif max(v) < 90 and max(v) - min(v) < 60:
                    p[x,y] = (0,0,0)
                elif max(v) < 130 and max(v) - min(v) >= 60:
                    # dunkle GESAETTIGTE Pixel (olivfarbenes AA der Kontur
                    # Richtung Gelb) dithern sonst zu gruen/roten Einzelpixeln.
                    # Ungesaettigte Grautoene 90..130 bleiben ungesnappt, damit
                    # der Sturmwolken-Verlauf gedithert bleibt.
                    p[x,y] = (0,0,0)
        # 2. Pass: der Anti-Aliasing-Halo der Konturen (ungesaettigte Mitteltoene
        # DIREKT NEBEN Schwarz) wuerde sonst zu bunten Dither-Sprenkeln entlang
        # der Kanten -- nach Helligkeit auf Schwarz/Weiss snappen. Flaechige
        # Grauverlaeufe ohne Schwarz-Nachbarn (Nebelbalken) bleiben unberuehrt.
        for _ in range(2):
            snap = []
            for y in range(SIZE):
                for x in range(SIZE):
                    v = p[x,y]
                    if not (max(v) - min(v) < 60 and 0 < max(v) <= WHITE_THR):
                        continue
                    near_black = any(p[nx,ny] == (0,0,0)
                                     for nx in range(max(0,x-1), min(SIZE,x+2))
                                     for ny in range(max(0,y-1), min(SIZE,y+2)))
                    if near_black:
                        snap.append((x, y, (0,0,0) if max(v) < 170 else (255,255,255)))
            for x, y, col in snap:
                p[x,y] = col
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
