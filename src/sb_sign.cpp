#include "sb_sign.h"
#if __has_include(<mbedtls/md.h>)
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
std::string sbSign(const std::string& token, const std::string& secret,
                   const std::string& t, const std::string& nonce){
  std::string msg = token + t + nonce;
  unsigned char mac[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(info,(const unsigned char*)secret.data(),secret.size(),
                  (const unsigned char*)msg.data(),msg.size(),mac);
  unsigned char b64[64]; size_t olen=0;
  mbedtls_base64_encode(b64,sizeof(b64),&olen,mac,32);
  return std::string((char*)b64,olen);
}
#else
// mbedTLS nicht verfuegbar (native Host ohne mbedTLS-Installation).
// Echte Implementierung laeuft nur auf dem Geraet (ESP32 hat mbedTLS built-in).
std::string sbSign(const std::string&, const std::string&,
                   const std::string&, const std::string&){
  return ""; // Stub fuer native Build
}
#endif
