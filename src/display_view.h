#pragma once
#include "reading.h"
struct HeaderInfo {
  float localTemp; int localHum;
  int battPct; bool charging;
  int hour; int minute;
  bool wifiOk;
};
void displayInit();
void displayRender(const SensorReading* r, int n, const HeaderInfo& h);
