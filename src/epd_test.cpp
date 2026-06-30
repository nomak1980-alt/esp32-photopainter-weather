// Task 3 Bring-up: GxEPD2-Testbild auf dem 7.3" E6-Panel.
// Nur im [env:epdtest] gebaut (build_src_filter).
#include <Arduino.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "config.h"

GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT> display(
  GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("EPD test start");
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    uint16_t cols[6] = {GxEPD_BLACK, GxEPD_RED, GxEPD_YELLOW,
                        GxEPD_BLUE, GxEPD_GREEN, GxEPD_WHITE};
    for (int i = 0; i < 6; i++) display.fillRect(0, i * 80, 800, 80, cols[i]);
    display.setFont(&FreeSansBold24pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(40, 120);
    display.print("PhotoPainter OK");
  } while (display.nextPage());
  Serial.println("EPD draw done");
}

void loop() {}
