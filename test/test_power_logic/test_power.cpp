#include <unity.h>
#include "power_logic.h"

// --- Schlafintervalle -------------------------------------------------
void test_day_interval() {
  TEST_ASSERT_EQUAL_UINT32(600u, sleepSeconds(5));    // 05-08 Uhr: 10 min
  TEST_ASSERT_EQUAL_UINT32(600u, sleepSeconds(7));
  TEST_ASSERT_EQUAL_UINT32(900u, sleepSeconds(8));    // 08-17 Uhr: 15 min
  TEST_ASSERT_EQUAL_UINT32(900u, sleepSeconds(12));
  TEST_ASSERT_EQUAL_UINT32(600u, sleepSeconds(17));   // 17-23 Uhr: 10 min
  TEST_ASSERT_EQUAL_UINT32(600u, sleepSeconds(22));
}

void test_night_interval() {
  TEST_ASSERT_EQUAL_UINT32(1800u, sleepSeconds(23));  // 23-05 Uhr: 30 min
  TEST_ASSERT_EQUAL_UINT32(1800u, sleepSeconds(2));
}

void test_boundaries() {
  TEST_ASSERT_EQUAL_UINT32(1800u, sleepSeconds(4));   // noch Nacht
  TEST_ASSERT_EQUAL_UINT32(600u,  sleepSeconds(5));   // Morgen beginnt
  TEST_ASSERT_EQUAL_UINT32(900u,  sleepSeconds(8));   // Tagbetrieb
  TEST_ASSERT_EQUAL_UINT32(900u,  sleepSeconds(16));
  TEST_ASSERT_EQUAL_UINT32(600u,  sleepSeconds(17));  // Abend
  TEST_ASSERT_EQUAL_UINT32(1800u, sleepSeconds(23));  // Nacht
}

void test_low_battery_doubles() {
  TEST_ASSERT_EQUAL_UINT32(1800u, sleepSeconds(12, 20, false));  // <= 20 % -> x2
  TEST_ASSERT_EQUAL_UINT32(3600u, sleepSeconds(2,   5, false));  // nachts ebenso
  TEST_ASSERT_EQUAL_UINT32(900u,  sleepSeconds(12, 21, false));  // knapp darueber
  TEST_ASSERT_EQUAL_UINT32(900u,  sleepSeconds(12, 20, true));   // am Ladegeraet
  TEST_ASSERT_EQUAL_UINT32(900u,  sleepSeconds(12, -1, false));  // unbekannt
}

// --- Refresh-Ausloeser ------------------------------------------------
static void mk(SensorReading& r, float t, int h) {
  r.valid = true; r.temperature = t; r.humidity = h; r.battery = 90;
}

void test_any_changed() {
  SensorReading now[4]{}, prev[4]{};
  for (int i = 0; i < 4; i++) { mk(now[i], 20.0f, 50); mk(prev[i], 20.0f, 50); }
  TEST_ASSERT_FALSE(anyChanged(now, prev, 4));

  now[0].temperature = 25.0f;                 // ein einzelner Sensor reicht nicht
  TEST_ASSERT_FALSE(anyChanged(now, prev, 4));

  now[1].temperature = 20.2f;                 // zweiter Sensor ueber der Schwelle
  TEST_ASSERT_TRUE(anyChanged(now, prev, 4));
}

void test_temp_threshold() {
  SensorReading now[2]{}, prev[2]{};
  for (int i = 0; i < 2; i++) { mk(now[i], 20.0f, 50); mk(prev[i], 20.0f, 50); }
  now[0].temperature = 20.1f; now[1].temperature = 19.9f;   // je 0,1 K -> zu wenig
  TEST_ASSERT_FALSE(anyChanged(now, prev, 2));
  now[0].temperature = 20.2f; now[1].temperature = 19.8f;   // je 0,2 K -> Refresh
  TEST_ASSERT_TRUE(anyChanged(now, prev, 2));
}

void test_hum_threshold() {
  SensorReading now[3]{}, prev[3]{};
  for (int i = 0; i < 3; i++) { mk(now[i], 20.0f, 50); mk(prev[i], 20.0f, 50); }
  now[0].humidity = 70;                       // ein Sensor, egal wie stark
  TEST_ASSERT_FALSE(anyChanged(now, prev, 3));
  now[1].humidity = 54;                       // zweiter nur 4 % -> zu wenig
  TEST_ASSERT_FALSE(anyChanged(now, prev, 3));
  now[1].humidity = 55;                       // 5 % -> Refresh
  TEST_ASSERT_TRUE(anyChanged(now, prev, 3));
}

void test_validity_change() {
  SensorReading now[2]{}, prev[2]{};
  for (int i = 0; i < 2; i++) { mk(now[i], 20.0f, 50); mk(prev[i], 20.0f, 50); }
  now[1].valid = false;                       // Sensor faellt weg -> sofort
  TEST_ASSERT_TRUE(anyChanged(now, prev, 2));
}

void test_battery_alone_no_refresh() {
  SensorReading now[2]{}, prev[2]{};
  for (int i = 0; i < 2; i++) { mk(now[i], 20.0f, 50); mk(prev[i], 20.0f, 50); }
  now[0].battery = 40; now[1].battery = 30;   // Sensorakku allein zeichnet nicht neu
  TEST_ASSERT_FALSE(anyChanged(now, prev, 2));
}

void test_single_sensor_setup() {
  SensorReading now[1]{}, prev[1]{};
  mk(now[0], 20.0f, 50); mk(prev[0], 20.0f, 50);
  TEST_ASSERT_FALSE(anyChanged(now, prev, 1));
  now[0].temperature = 20.2f;                 // bei nur einem Sensor genuegt dieser
  TEST_ASSERT_TRUE(anyChanged(now, prev, 1));
}
