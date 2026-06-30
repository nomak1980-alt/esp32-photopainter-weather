#include <unity.h>

void setUp() {}
void tearDown() {}

void test_sign_known_vector();

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sign_known_vector);
  return UNITY_END();
}
