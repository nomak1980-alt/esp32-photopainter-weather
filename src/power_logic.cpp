#include "power_logic.h"
uint32_t sleepSeconds(int hour){ return (hour>=0 && hour<5) ? 1800u : 600u; }
bool anyChanged(const SensorReading* now,const SensorReading* prev,int n){
  for(int i=0;i<n;i++) if(!sameValues(now[i],prev[i])) return true;
  return false;
}
