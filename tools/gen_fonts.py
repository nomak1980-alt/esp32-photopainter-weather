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

    # GFXglyph-Feldbreiten pruefen (Adafruit-GFX-Format):
    # bitmapOffset: uint16_t, width/height/xAdvance: uint8_t, xOffset/yOffset: int8_t
    for off, w, h, xadv, xo, yo in table:
        assert 0 <= off <= 65535, f"{name}: bitmapOffset {off} ausserhalb uint16"
        assert 0 <= w <= 255, f"{name}: width {w} ausserhalb uint8"
        assert 0 <= h <= 255, f"{name}: height {h} ausserhalb uint8"
        assert 0 <= xadv <= 255, f"{name}: xAdvance {xadv} ausserhalb uint8"
        assert -128 <= xo <= 127, f"{name}: xOffset {xo} ausserhalb int8"
        assert -128 <= yo <= 127, f"{name}: yOffset {yo} ausserhalb int8"

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
