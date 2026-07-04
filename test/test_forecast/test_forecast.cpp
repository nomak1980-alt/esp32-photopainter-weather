#include <unity.h>
#include "forecast.h"

void test_wmo_to_icon() {
  TEST_ASSERT_EQUAL(ICON_SUN,    wmoToIcon(0));
  TEST_ASSERT_EQUAL(ICON_PARTLY, wmoToIcon(2));
  TEST_ASSERT_EQUAL(ICON_CLOUD,  wmoToIcon(3));
  TEST_ASSERT_EQUAL(ICON_CLOUD,  wmoToIcon(45));
  TEST_ASSERT_EQUAL(ICON_RAIN,   wmoToIcon(61));
  TEST_ASSERT_EQUAL(ICON_RAIN,   wmoToIcon(80));
  TEST_ASSERT_EQUAL(ICON_SNOW,   wmoToIcon(75));
  TEST_ASSERT_EQUAL(ICON_STORM,  wmoToIcon(95));
}

void test_weekday() {
  // bekannte Anker
  TEST_ASSERT_EQUAL_INT(1, weekdayFromDate(2024, 1, 1));  // Montag
  TEST_ASSERT_EQUAL_INT(3, weekdayFromDate(2025, 1, 1));  // Mittwoch
  TEST_ASSERT_EQUAL_INT(2, weekdayFromDate(2026, 6, 30)); // Dienstag (Screenshot)
}

void test_parse_forecast() {
  const char* body = R"({"daily":{"time":["2026-06-30","2026-07-01"],
    "weather_code":[0,95],"temperature_2m_max":[34.4,30.1],
    "temperature_2m_min":[21.2,17.0]}})";
  DayForecast d[7];
  int count = 0;
  TEST_ASSERT_TRUE(parseForecastJson(body, d, 7, &count));
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_EQUAL_INT(0, d[0].wmoCode);
  TEST_ASSERT_EQUAL_INT(34, d[0].tMax);
  TEST_ASSERT_EQUAL_INT(21, d[0].tMin);
  TEST_ASSERT_EQUAL_INT(2, d[0].wday);
  TEST_ASSERT_EQUAL(ICON_SUN, wmoToIcon(d[0].wmoCode));
  TEST_ASSERT_EQUAL(ICON_STORM, wmoToIcon(d[1].wmoCode));
}

void test_parse_forecast_bad() {
  DayForecast d[7]; int count = -1;
  TEST_ASSERT_FALSE(parseForecastJson("{\"foo\":1}", d, 7, &count));
  TEST_ASSERT_EQUAL_INT(0, count);
}

void test_parse_hourly() {
  const char* body = R"({"hourly":{"time":["2026-07-04T14:00","2026-07-04T15:00"],
    "weather_code":[2,61],"temperature_2m":[27.6,26.2],
    "precipitation":[0.0,1.4]}})";
  HourForecast h[8];
  int count = 0;
  TEST_ASSERT_TRUE(parseHourlyJson(body, h, 8, &count));
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_EQUAL_INT(14, h[0].hour);
  TEST_ASSERT_EQUAL_INT(28, h[0].temp);          // 27.6 gerundet
  TEST_ASSERT_EQUAL(ICON_PARTLY, wmoToIcon(h[0].wmoCode));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, h[0].precipMm);
  TEST_ASSERT_EQUAL_INT(15, h[1].hour);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.4f, h[1].precipMm);
  TEST_ASSERT_EQUAL(ICON_RAIN, wmoToIcon(h[1].wmoCode));
}

void test_parse_hourly_bad() {
  HourForecast h[8]; int count = -1;
  TEST_ASSERT_FALSE(parseHourlyJson("{\"foo\":1}", h, 8, &count));
  TEST_ASSERT_EQUAL_INT(0, count);
}
