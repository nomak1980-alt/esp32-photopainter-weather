#include <unity.h>
#include "switchbot_api.h"

static const char* OK_JSON = R"({
  "statusCode": 100,
  "body": {
    "temperature": 21.5,
    "humidity": 60,
    "battery": 75
  }
})";

static const char* ERR_JSON = R"({
  "statusCode": 190,
  "body": {}
})";

void test_parse_ok() {
  SensorReading r{};
  bool ok = parseStatusJson(OK_JSON, r);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, r.temperature);
  TEST_ASSERT_EQUAL_INT(60, r.humidity);
  TEST_ASSERT_EQUAL_INT(75, r.battery);
}

void test_parse_error_body() {
  SensorReading r{};
  bool ok = parseStatusJson(ERR_JSON, r);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_FALSE(r.valid);
}
