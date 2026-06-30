#include <unity.h>
#include "view_model.h"
#include <cstring>

void test_temp_color() {
  TEST_ASSERT_EQUAL_INT(COL_BLUE,  tempColor(5.0f));
  TEST_ASSERT_EQUAL_INT(COL_BLUE,  tempColor(9.9f));
  TEST_ASSERT_EQUAL_INT(COL_GREEN, tempColor(10.0f));
  TEST_ASSERT_EQUAL_INT(COL_GREEN, tempColor(20.0f));
  TEST_ASSERT_EQUAL_INT(COL_GREEN, tempColor(25.0f));
  TEST_ASSERT_EQUAL_INT(COL_RED,   tempColor(25.1f));
  TEST_ASSERT_EQUAL_INT(COL_RED,   tempColor(35.0f));
}

void test_batt_warn() {
  TEST_ASSERT_TRUE (batteryWarn(0));
  TEST_ASSERT_TRUE (batteryWarn(19));
  TEST_ASSERT_FALSE(batteryWarn(20));
  TEST_ASSERT_FALSE(batteryWarn(100));
  TEST_ASSERT_FALSE(batteryWarn(-1));
}

void test_fmt() {
  SensorReading r{};
  char buf[32];

  // invalid reading
  r.valid = false;
  fmtTemp(r, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("-- --", buf);
  fmtHum (r, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("--",    buf);
  fmtBatt(r, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING(" ",     buf);

  // valid reading
  r.valid = true;
  r.temperature = 21.3f;
  r.humidity = 55;
  r.battery = 80;
  fmtTemp(r, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("21.3",    buf);
  fmtHum (r, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("55",      buf);
  fmtBatt(r, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING("Batt 80%", buf);

  // battery < 0
  r.battery = -1;
  fmtBatt(r, buf, sizeof(buf)); TEST_ASSERT_EQUAL_STRING(" ", buf);
}
