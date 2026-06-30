#pragma once
// =====================================================================
//  Hardware-Konfiguration ESP32-S3-PhotoPainter
// =====================================================================

// --- I2C (bestaetigt per Scan in Task 2, 2026-06-30) ---
//   Gefundene Adressen: 0x18 ES8311(Audio), 0x34 AXP2101(PMIC),
//   0x40 ES7210(Audio), 0x51 PCF85063(RTC), 0x70 SHTC3.
#define I2C_SDA_PIN        47
#define I2C_SCL_PIN        48
#define SHTC3_I2C_ADDR     0x70
#define PCF85063_I2C_ADDR  0x51
#define PMIC_I2C_ADDR      0x34   // AXP2101 (bestaetigt) -> XPowersLib

// --- E-Paper SPI (bestaetigt aus Repo user_app.cpp in Task 3) ---
//   ePaperPort(dither, mosi=11, scl=10, dc=8, cs=9, rst=12, busy=13, 800,480,...)
#define EPD_MOSI_PIN 11
#define EPD_SCK_PIN  10
#define EPD_DC_PIN   8
#define EPD_CS_PIN   9
#define EPD_RST_PIN  12
#define EPD_BUSY_PIN 13

// --- Sensoren (deviceId = MAC ohne Doppelpunkte, Grossbuchstaben) ---
#define DEVICE_COUNT 4
static const char* const DEVICE_IDS[DEVICE_COUNT] =
  {"EC2E85066871","EC2E8486410D","ED2E8606668D","B0E9FEF1FB2A"};
// Anzeigenamen ASCII-sicher (Adafruit-GFX-Standardfonts ohne Umlaute):
static const char* const DEVICE_NAMES[DEVICE_COUNT] =
  {"Aussen Hinten","Aussen Vorne","Buero","Kueche"};
static const bool DEVICE_OUTDOOR[DEVICE_COUNT] = {true,true,false,false};
