#pragma once
#include <cmath>
#include <cstring>
struct SensorReading {
  char  id[16];
  float temperature;
  int   humidity;
  int   battery;
  bool  valid;
};
inline int tenths(float t){ return (int)lround(t*10.0); }
inline bool sameValues(const SensorReading& a, const SensorReading& b){
  if (a.valid != b.valid) return false;
  if (!a.valid) return true;
  return tenths(a.temperature)==tenths(b.temperature)
      && a.humidity==b.humidity && a.battery==b.battery;
}
