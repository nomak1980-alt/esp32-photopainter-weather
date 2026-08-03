#pragma once
#include <cstdint>
#include "reading.h"

// Schlafdauer bis zum naechsten Update:
//   05-08 Uhr 10 min | 08-17 Uhr 15 min | 17-23 Uhr 10 min | 23-05 Uhr 30 min
// battPct < 0 = unbekannt. Ab <= 20 % Akku (und nicht am Ladegeraet)
// verdoppelt sich das Intervall (Sparmodus).
uint32_t sleepSeconds(int hour, int battPct = -1, bool charging = false);

// true = Anzeige neu zeichnen. Ausgeloest, wenn ein Sensor auftaucht/wegfaellt
// oder sich mindestens zwei Sensoren sichtbar geaendert haben
// (>= 0,2 K Temperatur bzw. >= 5 % Luftfeuchte).
bool anyChanged(const SensorReading* now, const SensorReading* prev, int n);
