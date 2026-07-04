#include <unity.h>
void setUp() {}
void tearDown() {}
void test_wmo_to_icon();
void test_weekday();
void test_parse_forecast();
void test_parse_forecast_bad();
void test_parse_hourly();
void test_parse_hourly_bad();
void test_wmo_to_icon_night();
void test_parse_hourly_is_day();
void test_parse_hourly_is_day_missing_defaults_day();
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_wmo_to_icon);
  RUN_TEST(test_weekday);
  RUN_TEST(test_parse_forecast);
  RUN_TEST(test_parse_forecast_bad);
  RUN_TEST(test_parse_hourly);
  RUN_TEST(test_parse_hourly_bad);
  RUN_TEST(test_wmo_to_icon_night);
  RUN_TEST(test_parse_hourly_is_day);
  RUN_TEST(test_parse_hourly_is_day_missing_defaults_day);
  return UNITY_END();
}
