#include "display_view.h"
#include <GxEPD2_7C.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "config.h"
#include "view_model.h"
#include "forecast.h"
#include "weather_icons.h"
#include <math.h>

static GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT> display(
  GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

static uint16_t toGx(Col c) {
  switch (c) {
    case COL_RED:    return GxEPD_RED;
    case COL_BLUE:   return GxEPD_BLUE;
    case COL_GREEN:  return GxEPD_GREEN;
    case COL_YELLOW: return GxEPD_YELLOW;
    case COL_WHITE:  return GxEPD_WHITE;
    default:         return GxEPD_BLACK;
  }
}

// GxEPD2 nimmt fuer dieses Panel die falsche BUSY-Polaritaet an.
// Waveshare-Treiber: Panel ist FERTIG wenn BUSY=HIGH, BUSY (beschaeftigt) wenn LOW.
// Diese Routine wartet korrekt, solange BUSY LOW ist (max. 30 s).
static void epdBusyWait(const void*) {
  delay(20);                       // BUSY-Pegel setzen lassen
  uint32_t t0 = millis();
  while (digitalRead(EPD_BUSY_PIN) == LOW) {
    if (millis() - t0 > 30000) break;
    delay(10);
  }
}

void displayInit() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display.init(115200);
  display.setRotation(2);   // 180 gedreht (Montage-Ausrichtung)
  display.epd2.setBusyCallback(epdBusyWait);
}

// Zeichnet "<num>°C" mit echtem Gradring (Fonts haben kein 0xB0).
// Cursor steht nach Aufruf hinter dem "C".
static void printTempC(int x, int baselineY, const char* numbuf,
                       int radius, int circleDY, uint16_t color) {
  display.setTextColor(color);
  display.setCursor(x, baselineY);
  display.print(numbuf);
  int cx = display.getCursorX();
  display.fillCircle(cx + radius + 3, baselineY - circleDY, radius, color);
  display.fillCircle(cx + radius + 3, baselineY - circleDY, radius - 2, GxEPD_WHITE);
  display.setCursor(cx + 2 * radius + 8, baselineY);
  display.print("C");
}

// Gibt UTF-8-Text aus. Die GFX-Fonts sind reines ASCII (7b), daher werden
// Umlaute als Basisbuchstabe mit zwei aufgemalten Punkten gezeichnet, ß als "ss".
// Punktposition/-groesse werden aus den Glyphen-Metriken des aktuellen Fonts abgeleitet.
static void printUtf8(const char* s, uint16_t color) {
  while (*s) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) { display.write((uint8_t)c); s++; continue; }
    unsigned char c2 = (unsigned char)s[1];
    if ((c & 0xE0) != 0xC0 || (c2 & 0xC0) != 0x80) { s++; continue; }
    unsigned cp = ((c & 0x1F) << 6) | (c2 & 0x3F);
    s += 2;
    char base;
    switch (cp) {
      case 0xE4: base = 'a'; break;  // ä
      case 0xF6: base = 'o'; break;  // ö
      case 0xFC: base = 'u'; break;  // ü
      case 0xC4: base = 'A'; break;  // Ä
      case 0xD6: base = 'O'; break;  // Ö
      case 0xDC: base = 'U'; break;  // Ü
      case 0xDF: display.print("ss"); continue;  // ß
      default: continue;             // unbekanntes Zeichen ueberspringen
    }
    int x0 = display.getCursorX(), y0 = display.getCursorY();
    char tmp[2] = {base, 0};
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(tmp, x0, y0, &bx, &by, &bw, &bh);
    display.write((uint8_t)base);
    int cx = bx + bw / 2;
    int dotY = by - 4;               // knapp ueber der Glyphen-Oberkante
    int r = bh >= 20 ? 3 : 2;
    display.fillCircle(cx - r - 2, dotY, r, color);
    display.fillCircle(cx + r + 2, dotY, r, color);
  }
}

static void drawTile(int x, int y, int w, int h, const char* name, const SensorReading& r) {
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  display.setTextSize(1);
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x + 14, y + 30);
  printUtf8(name, GxEPD_BLACK);
  display.setCursor(x + w - 58, y + 30);
  display.setTextColor(r.valid ? GxEPD_GREEN : GxEPD_BLACK);
  display.print(r.valid ? "ON" : "--");
  char buf[16];
  if (r.valid) {
    // grosse Temperatur (~2x): 18pt-Font doppelt skaliert
    char hum[8];
    fmtTemp(r, buf, sizeof buf);
    display.setFont(&FreeSansBold18pt7b);
    display.setTextSize(2);
    printTempC(x + 18, y + 96, buf, 7, 38, toGx(tempColor(r.temperature)));
    int hx = display.getCursorX();
    // Feuchte rechts neben Temperatur, mittelgross
    display.setTextSize(1);
    fmtHum(r, hum, sizeof hum);
    display.setFont(&FreeSansBold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(hx + 40, y + 96);
    display.print(hum); display.print("%");
    fmtBatt(r, buf, sizeof buf);
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(batteryWarn(r.battery) ? GxEPD_RED : GxEPD_BLACK);
    display.setCursor(x + 18, y + h - 14);   // statt y + 128
    display.print(buf);
  } else {
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x + 18, y + 90);
    display.print("-- keine Daten --");
  }
}

// ---------- Wetter-Icons (mit GFX-Primitiven, 6 Farben) ----------
static const char* WDAY_DE[7] = {"So","Mo","Di","Mi","Do","Fr","Sa"};

// Palettenindex 0..5 -> GxEPD-Farbe (siehe weather_icons.h)
static uint16_t iconColor(unsigned char idx) {
  switch (idx) {
    case 0: return GxEPD_BLACK;
    case 1: return GxEPD_WHITE;
    case 2: return GxEPD_RED;
    case 3: return GxEPD_YELLOW;
    case 4: return GxEPD_BLUE;
    case 5: return GxEPD_GREEN;
    default: return GxEPD_WHITE;
  }
}
// Wetter-Icon (gedithertes 6-Farben-Bitmap, 4 Bit/Pixel) pixelweise zeichnen.
// Weiss (Index 1) = transparent (Papierhintergrund).
static void drawWeatherIcon(int cx, int cy, WIcon ic) {
  const unsigned char* d = WICON_DATA[(int)ic];
  int x0 = cx - WICON_W / 2, y0 = cy - WICON_H / 2;
  int total = WICON_W * WICON_H;
  for (int p = 0; p < total; p++) {
    unsigned char byte = d[p >> 1];
    unsigned char idx = (p & 1) ? (byte & 0x0F) : (byte >> 4);
    if (idx == 1) continue;
    display.drawPixel(x0 + (p % WICON_W), y0 + (p / WICON_W), iconColor(idx));
  }
}

static void drawForecastBar(int y0, const DayForecast* fc, int count) {
  display.drawLine(0, y0, 800, y0, GxEPD_BLACK);
  int days = count < FORECAST_DAYS ? count : FORECAST_DAYS;
  if (days <= 0) return;
  int colW = 800 / days;
  for (int i = 0; i < days; i++) {
    int cx = i * colW + colW / 2;
    if (i > 0) display.drawLine(i * colW, y0 + 6, i * colW, y0 + 90, GxEPD_BLACK);
    // Wochentag
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    const char* wd = WDAY_DE[fc[i].wday % 7];
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(wd, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor(i * colW + 8, y0 + 22);   // linksbuendig
    display.print(wd);
    // Icon (groesser, nach rechts/oben gerueckt)
    drawWeatherIcon(cx + 16, y0 + 46, wmoToIcon(fc[i].wmoCode));
    // Temperaturen Max (schwarz) / Min (blau), je mit kleinem Gradring
    char hs[8], ls[8], full[20];
    snprintf(hs, sizeof hs, "%d", fc[i].tMax);
    snprintf(ls, sizeof ls, "%d", fc[i].tMin);
    snprintf(full, sizeof full, "%s   %s", hs, ls);  // grobe Breite fuer Zentrierung
    display.setFont(&FreeSansBold12pt7b);
    display.getTextBounds(full, 0, 0, &bx, &by, &bw, &bh);
    int tx = cx - bw / 2;
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(tx, y0 + 90);
    display.print(hs);
    int dx = display.getCursorX();
    display.drawCircle(dx + 3, y0 + 78, 2, GxEPD_BLACK);   // Gradring Max
    display.setCursor(dx + 9, y0 + 90);
    display.setTextColor(GxEPD_BLUE);
    display.print(ls);
    dx = display.getCursorX();
    display.drawCircle(dx + 3, y0 + 78, 2, GxEPD_BLUE);    // Gradring Min
  }
}

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

void displayRender(const SensorReading* r, int n, const HeaderInfo& hi,
                   const DayForecast* fc, int fcCount,
                   const HourForecast* hf, int hfCount) {
  (void)fc; (void)fcCount; (void)hf; (void)hfCount;  // je nach Modus unbenutzt
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    // --- Header ---
    display.setTextSize(1);
    display.setFont(&FreeSansBold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(12, 40);
    printUtf8(HEADER_TITLE, GxEPD_BLACK);
    // Untertitel: Uhrzeit + eigener Akku
    char sub[48];
    snprintf(sub, sizeof sub, "%02d:%02d    Akku %d%%%s",
             hi.hour, hi.minute, hi.battPct, hi.charging ? " +" : "");
    display.setFont(&FreeSans9pt7b);
    display.setCursor(14, 68);
    display.print(sub);
    if (!hi.wifiOk) {
      display.setTextColor(GxEPD_RED);
      display.print("   !WLAN");
      display.setTextColor(GxEPD_BLACK);
    }
    // Rechts: lokaler SHTC3-Wert gross
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(408, 56);
    display.print("Hier");
    char tnum[12];
    snprintf(tnum, sizeof tnum, "%.1f", hi.localTemp);
    display.setFont(&FreeSansBold18pt7b);
    display.setTextSize(2);
    printTempC(478, 66, tnum, 7, 38, toGx(tempColor(hi.localTemp)));
    int lhx = display.getCursorX();
    display.setTextSize(1);
    char loch[8];
    snprintf(loch, sizeof loch, "%d%%", hi.localHum);
    display.setFont(&FreeSansBold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(lhx + 38, 66);
    display.print(loch);
    display.drawLine(0, 82, 800, 82, GxEPD_BLACK);
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
  } while (display.nextPage());
}
