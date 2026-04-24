#include <mbedtls/aes.h>
#include <mbedtls/constant_time.h>
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>

#include "omemo.h"

// TODO: fix error handling

int omemoDriverHmac(const omemoKey k, const uint8_t *in, size_t ilen, uint8_t out[static 32]) {
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), k, 32, in, ilen, out);
  return 0;
}

int omemoDriverHkdf(const uint8_t *salt, size_t saltn, const uint8_t *key, size_t keyn, const uint8_t *info, size_t infon, uint8_t *out, size_t outn) {
  mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, saltn, key, keyn, info, infon, out, outn);
  return 0;
}

int omemoDriverGcmDecrypt(uint8_t *d, const uint8_t key[static 16], size_t n, const uint8_t iv[static 12], const uint8_t *tag, size_t tagn, const uint8_t *s) {
  int r;
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  if (!(r = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key,
                               128)))
    r = mbedtls_gcm_auth_decrypt(&ctx, n, iv, 12, "", 0, tag,
                                 tagn, s, d);
  mbedtls_gcm_free(&ctx);
  return 0;
}
