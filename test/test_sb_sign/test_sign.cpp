#include <unity.h>
#include "sb_sign.h"

void test_sign_known_vector() {
  std::string s = sbSign("TKN","SEC","1700000000000","abc");
  TEST_ASSERT_EQUAL_STRING("fT3GT5QlsTpYKKeoMAxmfUAtDb46o9Uxu0CD06oKWVU=", s.c_str());
}
