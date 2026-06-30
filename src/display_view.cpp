#include "display_view.h"
#include <GxEPD2_7C.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "config.h"
#include "view_model.h"

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

void displayInit() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display.init(115200);
  display.setRotation(2);   // 180 gedreht (Montage-Ausrichtung)
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
    printTempC(x + 18, y + 120, buf, 7, 38, toGx(tempColor(r.temperature)));
    int hx = display.getCursorX();
    // Feuchte rechts neben Temperatur, mittelgross
    display.setTextSize(1);
    fmtHum(r, hum, sizeof hum);
    display.setFont(&FreeSansBold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(hx + 40, y + 120);
    display.print(hum); display.print("%");
    fmtBatt(r, buf, sizeof buf);
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(batteryWarn(r.battery) ? GxEPD_RED : GxEPD_BLACK);
    display.setCursor(x + 18, y + 168);
    display.print(buf);
  } else {
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x + 18, y + 110);
    display.print("-- keine Daten --");
  }
}

void displayRender(const SensorReading* r, int n, const HeaderInfo& hi) {
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
    // --- 2x2 Kacheln ---
    int gx = 8, gy = 88, gw = (800 - 24) / 2, gh = (480 - 88 - 8) / 2;
    int pos = 0;
    for (int row = 0; row < 2 && pos < n; row++)
      for (int col = 0; col < 2 && pos < n; col++, pos++)
        drawTile(gx + col * (gw + 8), gy + row * (gh + 4), gw, gh, DEVICE_NAMES[pos], r[pos]);
  } while (display.nextPage());
}
