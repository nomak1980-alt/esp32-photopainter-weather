#include <unity.h>

void setUp() {}
void tearDown() {}

void test_same_values_both_invalid();
void test_same_values_one_invalid();
void test_same_values_equal();
void test_same_values_temp_diff();
void test_same_values_rounding();
void test_day_interval();
void test_night_interval();
void test_boundaries();
void test_low_battery_doubles();
void test_any_changed();
void test_temp_threshold();
void test_hum_threshold();
void test_validity_change();
void test_battery_alone_no_refresh();
void test_single_sensor_setup();

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_same_values_both_invalid);
  RUN_TEST(test_same_values_one_invalid);
  RUN_TEST(test_same_values_equal);
  RUN_TEST(test_same_values_temp_diff);
  RUN_TEST(test_same_values_rounding);
  RUN_TEST(test_day_interval);
  RUN_TEST(test_night_interval);
  RUN_TEST(test_boundaries);
  RUN_TEST(test_low_battery_doubles);
  RUN_TEST(test_any_changed);
  RUN_TEST(test_temp_threshold);
  RUN_TEST(test_hum_threshold);
  RUN_TEST(test_validity_change);
  RUN_TEST(test_battery_alone_no_refresh);
  RUN_TEST(test_single_sensor_setup);
  return UNITY_END();
}
