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
void test_any_changed();

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
  RUN_TEST(test_any_changed);
  return UNITY_END();
}
