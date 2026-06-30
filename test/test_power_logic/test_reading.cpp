#include <unity.h>
#include "reading.h"

void test_same_values_both_invalid() {
  SensorReading a{}, b{};
  a.valid = false; b.valid = false;
  TEST_ASSERT_TRUE(sameValues(a, b));
}

void test_same_values_one_invalid() {
  SensorReading a{}, b{};
  a.valid = true; b.valid = false;
  TEST_ASSERT_FALSE(sameValues(a, b));
}

void test_same_values_equal() {
  SensorReading a{}, b{};
  a.valid = b.valid = true;
  a.temperature = b.temperature = 21.3f;
  a.humidity = b.humidity = 55;
  a.battery = b.battery = 80;
  TEST_ASSERT_TRUE(sameValues(a, b));
}

void test_same_values_temp_diff() {
  SensorReading a{}, b{};
  a.valid = b.valid = true;
  a.temperature = 21.3f; b.temperature = 21.4f;
  a.humidity = b.humidity = 55;
  a.battery = b.battery = 80;
  TEST_ASSERT_FALSE(sameValues(a, b));
}

void test_same_values_rounding() {
  // 21.34 and 21.35 round to same tenth (213 vs 214 -> actually differ by 1 tenth)
  // 21.31 and 21.34 both round to 213
  SensorReading a{}, b{};
  a.valid = b.valid = true;
  a.temperature = 21.31f; b.temperature = 21.34f;
  a.humidity = b.humidity = 55;
  a.battery = b.battery = 80;
  TEST_ASSERT_TRUE(sameValues(a, b));
}
