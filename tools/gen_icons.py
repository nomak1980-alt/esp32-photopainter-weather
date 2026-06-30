#!/usr/bin/env python
# Konvertiert die ausgeschnittenen Wetter-Icons (icons_src/cut/*.png) in
# weather_icons.h: pro Pixel ein 6-Farben-Palettenindex (4 Bit, 2 px/Byte),
# Quantisierung "nearest" (kein Dither -> knackig). Plus Vorschau-PNG.
from PIL import Image
import os
CUT = "C:/Entwicklung/ESP32Wetter/icons_src/cut"
OUT_H = "C:/Entwicklung/ESP32Wetter/src/weather_icons.h"
names = ["sun","partly","cloud","rain","snow","storm"]   # = WIcon enum 0..5
SIZE = 46

# Panel-Palette (Index 0..5). Farbcode-Mapping fuer GxEPD in C:
#   0=BLACK 1=WHITE 2=RED 3=YELLOW 4=BLUE 5=GREEN
PAL = [(0,0,0),(255,255,255),(190,45,45),(235,190,40),(45,75,180),(50,140,60)]
palimg = Image.new("P",(1,1))
flat=[]
for c in PAL: flat += list(c)
flat += [0,0,0]*(256-len(PAL))
palimg.putpalette(flat)

def quant_indices(name):
    ic = Image.open(f"{CUT}/{name}.png").convert("RGBA")
    bg = Image.new("RGBA",(SIZE,SIZE),(255,255,255,255))
    bg.alpha_composite(ic.resize((SIZE,SIZE), Image.LANCZOS))
    q = bg.convert("RGB").quantize(palette=palimg, dither=0)
    return q  # mode P, Werte 0..5

# Vorschau
prev = Image.new("RGB",(SIZE*6, SIZE),(255,255,255))
icons_idx=[]
for i,n in enumerate(names):
    q = quant_indices(n)
    icons_idx.append(q)
    prev.paste(q.convert("RGB"),(i*SIZE,0))
prev.resize((prev.width*6, prev.height*6), Image.NEAREST).save(f"{CUT}/_final_preview.png")

# Header
lines=["#pragma once",
       "// AUTO-GENERIERT (tools/gen_icons.py) aus icons_src/cut/*.png",
       "// Pro Pixel ein Palettenindex 0..5 (4 Bit, 2 px/Byte, High-Nibble zuerst).",
       "// Index->Farbcode: 0=BLACK 1=WHITE 2=RED 3=YELLOW 4=BLUE 5=GREEN",
       f"#define WICON_W {SIZE}",
       f"#define WICON_H {SIZE}"]
for i,(n,q) in enumerate(zip(names, icons_idx)):
    px=list(q.getdata())
    packed=bytearray()
    for j in range(0,len(px),2):
        hi=px[j] & 0x0F
        lo=(px[j+1] & 0x0F) if j+1<len(px) else 0
        packed.append((hi<<4)|lo)
    body=",".join(str(b) for b in packed)
    lines.append(f"static const unsigned char IC_{i}[] = {{{body}}};")
lines.append("static const unsigned char* const WICON_DATA[6] = {"+",".join(f"IC_{i}" for i in range(6))+"};")
with open(OUT_H,"w") as f: f.write("\n".join(lines)+"\n")
print("OK ->", OUT_H, "| Vorschau:", f"{CUT}/_final_preview.png", "| bytes/icon:", (SIZE*SIZE+1)//2)
