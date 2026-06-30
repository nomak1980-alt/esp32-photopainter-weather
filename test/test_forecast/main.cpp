#include <unity.h>
void setUp() {}
void tearDown() {}
void test_wmo_to_icon();
void test_weekday();
void test_parse_forecast();
void test_parse_forecast_bad();
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_wmo_to_icon);
  RUN_TEST(test_weekday);
  RUN_TEST(test_parse_forecast);
  RUN_TEST(test_parse_forecast_bad);
  return UNITY_END();
}
