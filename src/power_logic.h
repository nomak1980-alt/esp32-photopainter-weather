#pragma once
#include <cstdint>
#include "reading.h"
uint32_t sleepSeconds(int hour);
bool anyChanged(const SensorReading* now,const SensorReading* prev,int n);
