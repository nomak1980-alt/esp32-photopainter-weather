#pragma once
#include "reading.h"
#include "forecast.h"
struct HeaderInfo {
  float localTemp; int localHum;
  int battPct; bool charging;
  int hour; int minute;
  bool wifiOk;
};
void displayInit();
void displayRender(const SensorReading* r, int n, const HeaderInfo& h,
                   const DayForecast* fc, int fcCount);
