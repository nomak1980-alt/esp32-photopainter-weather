#pragma once
// =====================================================================
//  Hardware-Konfiguration ESP32-S3-PhotoPainter (geraetespezifisch, nicht privat)
// =====================================================================

// --- I2C (bestaetigt per Scan in Task 2) ---
//   Gefundene Adressen: 0x18 ES8311(Audio), 0x34 AXP2101(PMIC),
//   0x40 ES7210(Audio), 0x51 PCF85063(RTC), 0x70 SHTC3.
#define I2C_SDA_PIN        47
#define I2C_SCL_PIN        48
#define SHTC3_I2C_ADDR     0x70
#define PCF85063_I2C_ADDR  0x51
#define PMIC_I2C_ADDR      0x34   // AXP2101 -> XPowersLib

// --- E-Paper SPI (aus Repo user_app.cpp) ---
//   ePaperPort(dither, mosi=11, scl=10, dc=8, cs=9, rst=12, busy=13, 800,480,...)
#define EPD_MOSI_PIN 11
#define EPD_SCK_PIN  10
#define EPD_DC_PIN   8
#define EPD_CS_PIN   9
#define EPD_RST_PIN  12
#define EPD_BUSY_PIN 13

// --- Vorhersage: Anzahl Tage/Stunden (Standort/Geraete in user_config.h) ---
#define FORECAST_DAYS  7
#define FORECAST_HOURS 8

// Persoenliche Konfiguration (Geraete-MACs, Standort) -> nicht im Repo.
// include/user_config.h aus user_config.example.h erstellen und ausfuellen.
#include "user_config.h"

// Defaults, falls eine aeltere user_config.h die neuen Schalter nicht kennt.
// DISPLAY_MODE: 0=Tages-Vorschau, 1=Stunden-Vorschau, 2=6 Thermometer ohne Leiste
#ifndef DISPLAY_MODE
#define DISPLAY_MODE 0
#endif

// =====================================================================
//  Energie-Stellschrauben (Akkulaufzeit)
// =====================================================================
// Wettervorhersage nur alle N Sekunden neu holen; dazwischen bleibt der
// Stand aus dem RTC-RAM stehen (spart pro Zyklus einen TLS-Handshake).
#ifndef FORECAST_INTERVAL_S
#define FORECAST_INTERVAL_S 1800
#endif
// Spaetestens nach N Minuten auch ohne Messwertaenderung neu zeichnen,
// damit "Werte aktualisiert um ..." nicht veraltet. 0 = aus.
#ifndef FORCE_REDRAW_MIN
#define FORCE_REDRAW_MIN 60
#endif
// Bitmaske der ALDO-Schienen, die vor dem Deep Sleep abgeschaltet werden.
//   Bit0=ALDO1, Bit1=ALDO2, Bit2=ALDO3, Bit3=ALDO4   (0 = nichts abschalten)
// STAND 31.07.2026: bewusst 0. Mit 0b1110 hing das Geraet ab dem zweiten
// Zyklus dauerhaft wach, der komplette I2C-Bus (SHTC3/AXP2101/PCF85063) war
// tot -- offenbar haengt daran mehr als gedacht (Pullups? RTC?). Vor einer
// erneuten Aktivierung Bit fuer Bit mit env:debug testen (30 s Zyklus,
// Serial an) und pruefen, ob nach dem Aufwachen "rtcReset=1" erscheint.
#ifndef SLEEP_OFF_ALDOS
#define SLEEP_OFF_ALDOS 0
#endif
