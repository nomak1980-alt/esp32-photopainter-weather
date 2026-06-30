#include <unity.h>

void setUp() {}
void tearDown() {}

void test_parse_ok();
void test_parse_error_body();

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_ok);
  RUN_TEST(test_parse_error_body);
  return UNITY_END();
}
