#include "power_logic.h"
#include <cstdlib>

// Ab wann eine Aenderung als "sichtbar genug fuer einen Refresh" gilt.
// Ein Panel-Refresh kostet ~30 s Wachzeit, darum bewusst traege.
static const int TEMP_DELTA_TENTHS = 2;   // 0,2 K
static const int HUM_DELTA_PCT     = 5;   // 5 %
static const int MIN_SENSORS       = 2;   // so viele Sensoren muessen sich bewegen

uint32_t sleepSeconds(int hour, int battPct, bool charging) {
  uint32_t sec;
  if      (hour >=  5 && hour <  8) sec =  600;   // 05-08 Uhr: 10 min
  else if (hour >=  8 && hour < 17) sec =  900;   // 08-17 Uhr: 15 min
  else if (hour >= 17 && hour < 23) sec =  600;   // 17-23 Uhr: 10 min
  else                              sec = 1800;   // 23-05 Uhr: 30 min
  if (battPct >= 0 && battPct <= 20 && !charging) sec *= 2;   // Sparmodus
  return sec;
}

bool anyChanged(const SensorReading* now, const SensorReading* prev, int n) {
  int need = n < MIN_SENSORS ? n : MIN_SENSORS;   // bei 1 Sensor reicht dieser
  int tempMoved = 0, humMoved = 0;
  for (int i = 0; i < n; i++) {
    if (now[i].valid != prev[i].valid) return true;   // Kachel wechselt den Inhalt
    if (!now[i].valid) continue;
    if (std::abs(tenths(now[i].temperature) - tenths(prev[i].temperature)) >= TEMP_DELTA_TENTHS)
      tempMoved++;
    if (std::abs(now[i].humidity - prev[i].humidity) >= HUM_DELTA_PCT)
      humMoved++;
  }
  return need > 0 && (tempMoved >= need || humMoved >= need);
}
