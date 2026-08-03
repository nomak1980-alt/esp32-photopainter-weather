#include "local_sensors.h"
#include <Wire.h>
#include <math.h>
#include <Adafruit_SHTC3.h>
#include "config.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

static Adafruit_SHTC3 shtc3;
static XPowersPMU pmu;

void localInit() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  // Erst den PMIC: die Schienen koennen aus dem Deep Sleep abgeschaltet sein,
  // daher vor allen anderen I2C-Teilnehmern wieder einschalten.
  pmu.begin(Wire, PMIC_I2C_ADDR, I2C_SDA_PIN, I2C_SCL_PIN);
  // Versorgungsschienen wie Werksfirmware aktivieren (E-Paper haengt an einer ALDO).
  pmu.setALDO1Voltage(3300); pmu.enableALDO1();
  pmu.setALDO2Voltage(3300); pmu.enableALDO2();
  pmu.setALDO3Voltage(3300); pmu.enableALDO3();
  pmu.setALDO4Voltage(3300); pmu.enableALDO4();
  delay(50);
  shtc3.begin(&Wire);
}

// Schaltet die per SLEEP_OFF_ALDOS markierten Schienen ab. Das E-Paper haelt
// sein Bild stromlos, die Audio-Codecs werden nie gebraucht -- nur die Schiene
// der PCF85063-RTC muss anbleiben (siehe Hinweis in config.h).
void localSleepPrepare() {
  if (SLEEP_OFF_ALDOS & 0b0001) pmu.disableALDO1();
  if (SLEEP_OFF_ALDOS & 0b0010) pmu.disableALDO2();
  if (SLEEP_OFF_ALDOS & 0b0100) pmu.disableALDO3();
  if (SLEEP_OFF_ALDOS & 0b1000) pmu.disableALDO4();
}

bool readSHTC3(float& t, int& h) {
  sensors_event_t hu, te;
  if (!shtc3.getEvent(&hu, &te)) return false;
  t = te.temperature;
  h = (int)lround(hu.relative_humidity);
  return true;
}

bool readBattery(int& pct, bool& chg) {
  pct = pmu.getBatteryPercent();
  chg = pmu.isCharging();
  return pct >= 0;
}

// PCF85063 (BCD-Register ab 0x04: sec,min,hour,day,wday,month,year)
static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

static bool s_rtcLostPower = false;   // OS-Flag der letzten Leseoperation

bool rtcLostPower() { return s_rtcLostPower; }

bool rtcNow(struct tm& o) {
  Wire.beginTransmission(PCF85063_I2C_ADDR);
  Wire.write(0x04);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(PCF85063_I2C_ADDR, 7) != 7) return false;
  uint8_t s = Wire.read(), mi = Wire.read(), hh = Wire.read(),
          dd = Wire.read(), wd = Wire.read(), mo = Wire.read(), yr = Wire.read();
  // Bit 7 des Sekundenregisters = OS: Oszillator stand -> Zeit ist Muell
  // (passiert, wenn die Schiene der RTC im Schlaf abgeschaltet wurde).
  s_rtcLostPower = (s & 0x80) != 0;
  if (s_rtcLostPower) return false;
  o.tm_sec  = bcd2dec(s & 0x7F);
  o.tm_min  = bcd2dec(mi & 0x7F);
  o.tm_hour = bcd2dec(hh & 0x3F);
  o.tm_mday = bcd2dec(dd & 0x3F);
  o.tm_mon  = bcd2dec(mo & 0x1F) - 1;
  o.tm_year = bcd2dec(yr) + 100;
  (void)wd;
  return true;
}

void rtcSet(const struct tm& t) {
  Wire.beginTransmission(PCF85063_I2C_ADDR);
  Wire.write(0x04);
  Wire.write(dec2bcd(t.tm_sec));
  Wire.write(dec2bcd(t.tm_min));
  Wire.write(dec2bcd(t.tm_hour));
  Wire.write(dec2bcd(t.tm_mday));
  Wire.write(dec2bcd(0));
  Wire.write(dec2bcd(t.tm_mon + 1));
  Wire.write(dec2bcd(t.tm_year - 100));
  Wire.endTransmission();
}
