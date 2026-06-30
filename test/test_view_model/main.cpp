#include <unity.h>

void setUp() {}
void tearDown() {}

void test_temp_color();
void test_batt_warn();
void test_fmt();

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_temp_color);
  RUN_TEST(test_batt_warn);
  RUN_TEST(test_fmt);
  return UNITY_END();
}
