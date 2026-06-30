#include <unity.h>
#include "power_logic.h"

void test_day_interval() {
  TEST_ASSERT_EQUAL_UINT32(600u, sleepSeconds(8));
}

void test_night_interval() {
  TEST_ASSERT_EQUAL_UINT32(1800u, sleepSeconds(2));
}

void test_boundaries() {
  TEST_ASSERT_EQUAL_UINT32(1800u, sleepSeconds(0));
  TEST_ASSERT_EQUAL_UINT32(1800u, sleepSeconds(4));
  TEST_ASSERT_EQUAL_UINT32(600u,  sleepSeconds(5));
  TEST_ASSERT_EQUAL_UINT32(600u,  sleepSeconds(23));
}

void test_any_changed() {
  SensorReading now[2]{}, prev[2]{};
  now[0].valid = prev[0].valid = true;
  now[0].temperature = prev[0].temperature = 20.0f;
  now[0].humidity = prev[0].humidity = 50;
  now[0].battery = prev[0].battery = 90;
  now[1].valid = prev[1].valid = true;
  now[1].temperature = prev[1].temperature = 18.0f;
  now[1].humidity = prev[1].humidity = 60;
  now[1].battery = prev[1].battery = 70;
  TEST_ASSERT_FALSE(anyChanged(now, prev, 2));

  now[1].temperature = 19.0f;
  TEST_ASSERT_TRUE(anyChanged(now, prev, 2));
}
