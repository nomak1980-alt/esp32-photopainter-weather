#pragma once
#include <time.h>
void localInit();
bool readSHTC3(float& temp, int& hum);
bool readBattery(int& pct, bool& charging);
bool rtcNow(struct tm& out);      // PCF85063
void rtcSet(const struct tm& t);
