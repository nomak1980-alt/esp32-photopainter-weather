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

// --- E-Paper SPI (wird in Task 3 aus Repo/Schaltplan bestaetigt) ---
// #define EPD_CS_PIN   <pin>
// #define EPD_DC_PIN   <pin>
// #define EPD_RST_PIN  <pin>
// #define EPD_BUSY_PIN <pin>
// #define EPD_SCK_PIN  <pin>
// #define EPD_MOSI_PIN <pin>
