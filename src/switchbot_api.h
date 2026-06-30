#pragma once
#include "reading.h"
bool parseStatusJson(const char* json, SensorReading& out);
int  fetchAll(const char* const* ids, SensorReading* readings, int n);
