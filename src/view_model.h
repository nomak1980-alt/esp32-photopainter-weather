#pragma once
#include <cstddef>
#include "reading.h"
enum Col { COL_BLACK, COL_RED, COL_YELLOW, COL_BLUE, COL_GREEN, COL_WHITE };
Col tempColor(float c);
bool batteryWarn(int battery);
void fmtTemp(const SensorReading&,char*,size_t);
void fmtHum (const SensorReading&,char*,size_t);
void fmtBatt(const SensorReading&,char*,size_t);
