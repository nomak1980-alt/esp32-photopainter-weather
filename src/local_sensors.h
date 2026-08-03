#pragma once
#include <time.h>
void localInit();
bool readSHTC3(float& temp, int& hum);
bool readBattery(int& pct, bool& charging);
bool rtcNow(struct tm& out);      // PCF85063
void rtcSet(const struct tm& t);
bool rtcLostPower();              // true = PCF85063 war stromlos, Zeit ungueltig
void localSleepPrepare();         // Schienen vor dem Deep Sleep abschalten
