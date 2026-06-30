#include "display_view.h"
#include <GxEPD2_7C.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "config.h"
#include "view_model.h"
#include "forecast.h"
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

static void drawTile(int x, int y, int w, int h, const char* name, const SensorReading& r) {
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  display.setTextSize(1);
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x + 14, y + 30);
  display.print(name);
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
    display.setCursor(x + 18, y + 128);
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

static void icSun(int cx, int cy, int r) {
  display.fillCircle(cx, cy, r, GxEPD_YELLOW);
  for (int a = 0; a < 360; a += 45) {
    float rad = a * 3.14159f / 180.0f;
    display.drawLine(cx + cosf(rad) * (r + 2), cy + sinf(rad) * (r + 2),
                     cx + cosf(rad) * (r + 5), cy + sinf(rad) * (r + 5), GxEPD_RED);
  }
}
static void icCloud(int cx, int cy, uint16_t col) {
  display.fillCircle(cx - 8, cy + 3, 7, col);
  display.fillCircle(cx + 8, cy + 3, 8, col);
  display.fillCircle(cx - 1, cy - 3, 9, col);
  display.fillRect(cx - 14, cy + 3, 30, 8, col);
}
static void icRainDrops(int cx, int cy, uint16_t col) {
  for (int i = -1; i <= 1; i++)
    display.drawLine(cx + i * 8, cy, cx + i * 8 - 2, cy + 7, col);
}
static void drawWeatherIcon(int cx, int cy, WIcon ic) {
  switch (ic) {
    case ICON_SUN:    icSun(cx, cy, 11); break;
    case ICON_PARTLY: icSun(cx - 7, cy - 6, 7); icCloud(cx + 4, cy + 3, GxEPD_BLACK); break;
    case ICON_CLOUD:  icCloud(cx, cy, GxEPD_BLACK); break;
    case ICON_RAIN:   icCloud(cx, cy - 4, GxEPD_BLACK); icRainDrops(cx, cy + 11, GxEPD_BLUE); break;
    case ICON_SNOW:   icCloud(cx, cy - 4, GxEPD_BLACK);
                      for (int i = -1; i <= 1; i++) display.fillCircle(cx + i * 8, cy + 12, 2, GxEPD_BLUE);
                      break;
    case ICON_STORM:  icCloud(cx, cy - 4, GxEPD_BLACK);
                      display.fillTriangle(cx - 2, cy + 4, cx + 5, cy + 4, cx - 1, cy + 12, GxEPD_YELLOW);
                      display.fillTriangle(cx + 1, cy + 9, cx + 6, cy + 9, cx - 2, cy + 18, GxEPD_YELLOW);
                      break;
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
    display.setCursor(cx - bw / 2, y0 + 24);
    display.print(wd);
    // Icon
    drawWeatherIcon(cx, y0 + 50, wmoToIcon(fc[i].wmoCode));
    // Temperaturen Max (schwarz) / Min (blau), je mit kleinem Gradring
    char hs[8], ls[8], full[20];
    snprintf(hs, sizeof hs, "%d", fc[i].tMax);
    snprintf(ls, sizeof ls, "%d", fc[i].tMin);
    snprintf(full, sizeof full, "%s   %s", hs, ls);  // grobe Breite fuer Zentrierung
    display.setFont(&FreeSansBold12pt7b);
    display.getTextBounds(full, 0, 0, &bx, &by, &bw, &bh);
    int tx = cx - bw / 2;
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(tx, y0 + 86);
    display.print(hs);
    int dx = display.getCursorX();
    display.drawCircle(dx + 3, y0 + 74, 2, GxEPD_BLACK);   // Gradring Max
    display.setCursor(dx + 9, y0 + 86);
    display.setTextColor(GxEPD_BLUE);
    display.print(ls);
    dx = display.getCursorX();
    display.drawCircle(dx + 3, y0 + 74, 2, GxEPD_BLUE);    // Gradring Min
  }
}

void displayRender(const SensorReading* r, int n, const HeaderInfo& hi,
                   const DayForecast* fc, int fcCount) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    // --- Header ---
    display.setTextSize(1);
    display.setFont(&FreeSansBold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(12, 40);
    display.print("SwitchBot Wetter");
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
    // --- 2x2 Kacheln (verkleinert, Platz fuer Wetterbalken unten) ---
    const int FC_Y = 384;
    int gx = 8, gy = 88, gw = (800 - 24) / 2, gh = (FC_Y - 88 - 4) / 2;
    int pos = 0;
    for (int row = 0; row < 2 && pos < n; row++)
      for (int col = 0; col < 2 && pos < n; col++, pos++)
        drawTile(gx + col * (gw + 8), gy + row * (gh + 4), gw, gh, DEVICE_NAMES[pos], r[pos]);
    // --- Wettervorhersage-Balken ---
    drawForecastBar(FC_Y, fc, fcCount);
  } while (display.nextPage());
}
