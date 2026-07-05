#include "display_view.h"
#include <GxEPD2_7C.h>
#include "big_fonts.h"
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

// Zeichnet "<num>°C" in Farbe; das Gradzeichen ist ein echtes Font-Glyph
// (Latin-1 0xB0). Cursor steht nach Aufruf hinter dem "C".
static void printTempC(int x, int baselineY, const char* numbuf, uint16_t color) {
  display.setTextColor(color);
  display.setCursor(x, baselineY);
  display.print(numbuf);
  display.print("\xB0" "C");
}

// Gibt UTF-8-Text aus. Die generierten Arial-Fonts sind Latin-1-indiziert
// (inkl. Umlaute, ß, °), daher genuegt UTF-8 -> Latin-1-Byte.
static void printUtf8(const char* s) {
  while (*s) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) { display.write((uint8_t)c); s++; continue; }
    unsigned char c2 = (unsigned char)s[1];
    if ((c & 0xE0) != 0xC0 || (c2 & 0xC0) != 0x80) { s++; continue; }
    unsigned cp = ((c & 0x1F) << 6) | (c2 & 0x3F);
    if (cp <= 0xFF) display.write((uint8_t)cp);   // ausserhalb Latin-1: skip
    s += 2;
  }
}

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
  display.setFont(&ArialBold12);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x + 14, y + 30);
  printUtf8(name);
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
    // Temperatur gross (36pt), Feuchte daneben (18pt, halbe Groesse)
    char hum[8];
    fmtTemp(r, buf, sizeof buf);
    display.setFont(&ArialBold36);
    printTempC(x + 18, y + h - 22, buf, toGx(tempColor(r.temperature)));
    int hx = display.getCursorX();
    fmtHum(r, hum, sizeof hum);
    display.setFont(&ArialBold18);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(hx + 34, y + h - 22);
    display.print(hum); display.print("%");
  } else {
    display.setFont(&Arial9);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x + 18, y + (h + 10) / 2);
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
    if (i > 0) display.drawLine(i * colW, y0 + 6, i * colW, y0 + 128, GxEPD_BLACK);
    // Wochentag
    display.setFont(&ArialBold12);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(i * colW + 8, y0 + 24);   // linksbuendig
    display.print(WDAY_DE[fc[i].wday % 7]);
    // Icon (mittig, leicht nach oben gerueckt)
    drawWeatherIcon(cx, y0 + 58, wmoToIcon(fc[i].wmoCode));
    // Temperaturen Max (schwarz) / Min (blau), zentriert
    char hs[8], ls[8], full[20];
    snprintf(hs, sizeof hs, "%d\xB0", fc[i].tMax);
    snprintf(ls, sizeof ls, "%d\xB0", fc[i].tMin);
    snprintf(full, sizeof full, "%s  %s", hs, ls);
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(full, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor(cx - bw / 2, y0 + 128);
    display.setTextColor(GxEPD_BLACK);
    display.print(hs);
    display.print("  ");
    display.setTextColor(GxEPD_BLUE);
    display.print(ls);
  }
}

// Stundenleiste: pro Spalte Uhrzeit, Icon, darunter Temperatur (Gradring)
// und in eigener Zeile der Niederschlag in mm (mit Nachkommastelle).
static void drawHourlyBar(int y0, const HourForecast* hf, int count) {
  display.drawLine(0, y0, 800, y0, GxEPD_BLACK);
  int cols = count < FORECAST_HOURS ? count : FORECAST_HOURS;
  if (cols <= 0) return;
  int colW = 800 / cols;
  for (int i = 0; i < cols; i++) {
    int cx = i * colW + colW / 2;
    if (i > 0) display.drawLine(i * colW, y0 + 6, i * colW, y0 + 130, GxEPD_BLACK);
    char hbuf[8];
    snprintf(hbuf, sizeof hbuf, "%02d:00", hf[i].hour);
    display.setFont(&ArialBold12);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(i * colW + 6, y0 + 24);
    display.print(hbuf);
    drawWeatherIcon(cx, y0 + 58, wmoToIconDN(hf[i].wmoCode, hf[i].isDay));
    // Temperatur, darunter Niederschlag blau
    char ts[8];
    snprintf(ts, sizeof ts, "%d\xB0", hf[i].temp);
    display.setCursor(i * colW + 6, y0 + 106);
    display.print(ts);
    char ps[12];
    snprintf(ps, sizeof ps, "%.1f mm", hf[i].precipMm);
    display.setFont(&Arial9);
    display.setTextColor(GxEPD_BLUE);
    display.setCursor(i * colW + 6, y0 + 130);
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
    display.setFont(&ArialBold18);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(12, 40);
    printUtf8(HEADER_TITLE);
    // Untertitel: Uhrzeit + eigener Akku
    char sub[48];
    snprintf(sub, sizeof sub, "%02d:%02d    Akku %d%%%s",
             hi.hour, hi.minute, hi.battPct, hi.charging ? " +" : "");
    display.setFont(&Arial9);
    display.setCursor(14, 68);
    display.print(sub);
    if (!hi.wifiOk) {
      display.setTextColor(GxEPD_RED);
      display.print("   !WLAN");
      display.setTextColor(GxEPD_BLACK);
    }
    // Rechts: lokaler SHTC3-Wert gross
    display.setFont(&ArialBold12);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(408, 56);
    display.print("Hier");
    char tnum[12];
    snprintf(tnum, sizeof tnum, "%.1f", hi.localTemp);
    display.setFont(&ArialBold36);
    printTempC(478, 66, tnum, toGx(tempColor(hi.localTemp)));
    int lhx = display.getCursorX();
    char loch[8];
    snprintf(loch, sizeof loch, "%d%%", hi.localHum);
    display.setFont(&ArialBold18);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(lhx + 34, 66);
    display.print(loch);
    display.drawLine(0, 82, 800, 82, GxEPD_BLACK);
    // --- Kachelraster: 2x2 mit Wetterleiste, 2x3 ohne (Modus 2) ---
#if DISPLAY_MODE == 2
    const int FC_Y = 480;
    const int ROWS = 3;
#else
    const int FC_Y = 344;
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
