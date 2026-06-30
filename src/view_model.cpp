#include "view_model.h"
#include <cstdio>
#include <cmath>
Col tempColor(float c){ if(c<10) return COL_BLUE; if(c>25) return COL_RED; return COL_GREEN; }
bool batteryWarn(int b){ return b>=0 && b<20; }
void fmtTemp(const SensorReading& r,char* o,size_t n){
  if(!r.valid){ snprintf(o,n,"-- --"); return; } snprintf(o,n,"%.1f",r.temperature); }
void fmtHum(const SensorReading& r,char* o,size_t n){
  if(!r.valid){ snprintf(o,n,"--"); return; } snprintf(o,n,"%d",r.humidity); }
void fmtBatt(const SensorReading& r,char* o,size_t n){
  if(!r.valid||r.battery<0){ snprintf(o,n," "); return; } snprintf(o,n,"Batt %d%%",r.battery); }
