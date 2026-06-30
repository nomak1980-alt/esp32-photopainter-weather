#!/usr/bin/env python
# Generiert Wetter-Icons als 1-Bit-Farbebenen-Bitmaps (Adafruit GFX drawBitmap, MSB first)
# + Vorschau-PNG. Icons: SUN, PARTLY, CLOUD, RAIN, SNOW, STORM (Reihenfolge = WIcon enum)
from PIL import Image, ImageDraw
import math, os

W = H = 48
S = 4                      # Supersampling
BW = (W + 7) // 8          # byte width per row

# Farbcodes: 0=BLACK,1=RED,2=YELLOW,3=BLUE,4=GREEN,5=WHITE
COL_RGB = {0:(0,0,0),1:(220,40,40),2:(245,200,30),3:(40,90,210),4:(40,170,60),5:(255,255,255)}

def new_layer():
    return Image.new("L", (W*S, H*S), 0)   # schwarz=0; wir zeichnen Form in weiss(255)

def to_bits(img):
    """L-Bild (Form in hell) -> 1-Bit, downscaled, threshold. Gibt (bytes, pil_mask)."""
    small = img.resize((W, H), Image.LANCZOS)
    px = small.load()
    data = bytearray(BW * H)
    mask = Image.new("1", (W, H), 0)
    mpx = mask.load()
    for y in range(H):
        for x in range(W):
            if px[x, y] >= 110:
                data[y*BW + x//8] |= (0x80 >> (x & 7))
                mpx[x, y] = 1
    return bytes(data), mask

# ---- Form-Helfer (Koordinaten in 48er-Raum, *S beim Zeichnen) ----
def d(img): return ImageDraw.Draw(img)
def disc(dr, cx, cy, r, fill=255):
    dr.ellipse([(cx-r)*S,(cy-r)*S,(cx+r)*S,(cy+r)*S], fill=fill)

def cloud_filled(dr, cx, cy, scale=1.0, fill=255):
    r = lambda v: v*scale
    disc(dr, cx-9*scale, cy+4*scale, r(8), fill)
    disc(dr, cx+9*scale, cy+4*scale, r(9), fill)
    disc(dr, cx-1*scale, cy-4*scale, r(11), fill)
    disc(dr, cx+6*scale, cy-2*scale, r(9), fill)
    dr.rectangle([(cx-17*scale)*S,(cy+1*scale)*S,(cx+16*scale)*S,(cy+12*scale)*S], fill=fill)

def cloud_outline_layer(cx, cy, scale=1.0):
    """Wolke als Rand (gefuellt minus innen)."""
    img = new_layer(); dr = d(img)
    cloud_filled(dr, cx, cy, scale, 255)
    inner = new_layer(); di = d(inner)
    cloud_filled(di, cx, cy, scale, 255)
    # innen schrumpfen: kleinere Wolke abziehen
    sub = new_layer(); ds = d(sub)
    cloud_filled(ds, cx, cy+0.6, scale*0.80, 255)
    # Rand = img AND NOT sub
    from PIL import ImageChops
    band = ImageChops.subtract(img, sub)
    return band

def rays_layer(cx, cy, r0, r1):
    img = new_layer(); dr = d(img)
    for a in range(0, 360, 45):
        rad = math.radians(a)
        x0, y0 = cx+math.cos(rad)*r0, cy+math.sin(rad)*r0
        x1, y1 = cx+math.cos(rad)*r1, cy+math.sin(rad)*r1
        dr.line([x0*S,y0*S,x1*S,y1*S], fill=255, width=int(2.4*S))
    return img

def drop(dr, x, y, r=3.2):
    dr.polygon([(x*S,(y-r*1.7)*S),((x-r)*S,(y+r*0.2)*S),((x+r)*S,(y+r*0.2)*S)], fill=255)
    disc(dr, x, y+r*0.4, r)

def bolt_layer(cx, cy):
    img = new_layer(); dr = d(img)
    pts = [(cx+2,cy-9),(cx-5,cy+2),(cx,cy+2),(cx-3,cy+11),(cx+7,cy-2),(cx+1,cy-2)]
    dr.polygon([(x*S,y*S) for x,y in pts], fill=255)
    return img

# ---- Icons als Liste von (colorCode, layer_image) ----
def ic_sun():
    L=[]
    L.append((1, rays_layer(24,24,15,23)))
    s=new_layer(); disc(d(s),24,24,13); L.append((2,s))
    return L
def ic_partly():
    L=[]
    L.append((1, rays_layer(16,16,9,15)))
    s=new_layer(); disc(d(s),16,16,9); L.append((2,s))
    cw=new_layer(); cloud_filled(d(cw),27,28,0.85); L.append((5,cw))   # weiss maskiert Sonne
    L.append((0, cloud_outline_layer(27,28,0.85)))
    return L
def ic_cloud():
    return [(0, cloud_outline_layer(24,22,1.0))]
def ic_rain():
    L=[(0, cloud_outline_layer(24,18,0.95))]
    dl=new_layer(); dr=d(dl)
    for i in (-1,0,1): drop(dr, 24+i*9, 36)
    L.append((3, dl)); return L
def ic_snow():
    L=[(0, cloud_outline_layer(24,18,0.95))]
    sl=new_layer(); ds=d(sl)
    for i in (-1,0,1):
        cx0=24+i*9; cy0=36
        for a in range(0,180,60):
            rad=math.radians(a)
            ds.line([(cx0-4*math.cos(rad))*S,(cy0-4*math.sin(rad))*S,
                     (cx0+4*math.cos(rad))*S,(cy0+4*math.sin(rad))*S], fill=255, width=int(1.4*S))
    L.append((3, sl)); return L
def ic_storm():
    L=[(0, cloud_outline_layer(24,18,0.95))]
    L.append((2, bolt_layer(24,34)))
    return L

ICONS = [ic_sun(), ic_partly(), ic_cloud(), ic_rain(), ic_snow(), ic_storm()]
NAMES = ["SUN","PARTLY","CLOUD","RAIN","SNOW","STORM"]

# ---- Vorschau PNG ----
prev = Image.new("RGB", (W*6+10*7, H+40), (255,255,255))
pd = ImageDraw.Draw(prev)
for idx, layers in enumerate(ICONS):
    tile = Image.new("RGB",(W,H),(255,255,255))
    for code, limg in layers:
        _, mask = to_bits(limg)
        solid = Image.new("RGB",(W,H),COL_RGB[code])
        tile.paste(solid, (0,0), mask)
    prev.paste(tile, (10+idx*(W+10), 10))
    pd.text((10+idx*(W+10), H+18), NAMES[idx], fill=(0,0,0))
prev = prev.resize((prev.width*3, prev.height*3), Image.NEAREST)
prev.save(os.path.join(os.path.dirname(__file__),"icons_preview.png"))

# ---- Header schreiben ----
out = []
out.append("#pragma once")
out.append("// AUTO-GENERIERT (gen_icons.py) - Wetter-Icons als 1-Bit Farbebenen")
out.append(f"#define WICON_W {W}")
out.append(f"#define WICON_H {H}")
arr_names = []
for idx, layers in enumerate(ICONS):
    for li,(code,limg) in enumerate(layers):
        data,_ = to_bits(limg)
        nm = f"IC_{idx}_{li}"
        arr_names.append(nm)
        body = ",".join(str(b) for b in data)
        out.append(f"static const uint8_t {nm}[] = {{{body}}};")
out.append("struct WLayer { const uint8_t* bmp; unsigned char color; };")
for idx, layers in enumerate(ICONS):
    items = ",".join("{IC_%d_%d,%d}"%(idx,li,code) for li,(code,_) in enumerate(layers))
    out.append(f"static const WLayer IC_LAYERS_{idx}[] = {{{items}}};")
out.append("static const WLayer* const IC_LAYERS[6] = {" + ",".join(f"IC_LAYERS_{i}" for i in range(6)) + "};")
out.append("static const unsigned char IC_NLAYERS[6] = {" + ",".join(str(len(l)) for l in ICONS) + "};")
hdr = "\n".join(out) + "\n"
with open(os.path.join("C:/Entwicklung/ESP32Wetter/src","weather_icons.h"),"w") as f:
    f.write(hdr)
print("OK - preview + weather_icons.h geschrieben. Layer-Zahlen:", [len(l) for l in ICONS])
