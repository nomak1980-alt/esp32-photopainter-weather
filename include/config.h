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
#ifndef HEADER_TITLE
#define HEADER_TITLE "SwitchBot Wetter"
#endif
