#include <unity.h>
#include "sb_sign.h"

void test_sign_known_vector() {
#if __has_include(<mbedtls/md.h>)
  std::string s = sbSign("TKN","SEC","1700000000000","abc");
  TEST_ASSERT_EQUAL_STRING("fT3GT5QlsTpYKKeoMAxmfUAtDb46o9Uxu0CD06oKWVU=", s.c_str());
#else
  TEST_IGNORE_MESSAGE("mbedTLS auf dem Host nicht verfuegbar - sb_sign wird on-device getestet");
#endif
}
