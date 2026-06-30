#include "switchbot_api.h"
#include <ArduinoJson.h>
bool parseStatusJson(const char* json, SensorReading& out){
  JsonDocument doc;
  if(deserializeJson(doc,json)) { out.valid=false; return false; }
  if(doc["statusCode"].as<int>()!=100){ out.valid=false; return false; }
  JsonObject b=doc["body"];
  if(b["temperature"].isNull()){ out.valid=false; return false; }
  out.temperature=b["temperature"].as<float>();
  out.humidity=b["humidity"].as<int>();
  out.battery=b["battery"].is<int>()?b["battery"].as<int>():-1;
  out.valid=true; return true;
}

#if !defined(ARDUINO)
int fetchAll(const char* const*, SensorReading*, int){ return 0; }
#endif
