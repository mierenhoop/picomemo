/**
 * Copyright 2026 mierenhoop
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <mbedtls/aes.h>
#include <mbedtls/constant_time.h>
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>

#include "omemo.h"
#include "driver.h"

#define TRY(r) do { if (r) return OMEMO_ECRYPTO; } while (0)

int omemoDriverHmac(const omemoKey k, const uint8_t *in, size_t ilen, uint8_t out[static 32]) {
  TRY(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), k, 32, in, ilen, out));
  return 0;
}


int omemoDriverAesEncrypt(omemoKey k, size_t n, uint8_t iv[static 16], const uint8_t *s, uint8_t *d) {
  mbedtls_aes_context aes;
  TRY(mbedtls_aes_setkey_enc(&aes, k, 256));
  TRY(mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, n, iv, s, d));
  return 0;
}

int omemoDriverAesDecrypt(omemoKey k, size_t n, uint8_t iv[static 16], const uint8_t *s, uint8_t *d) {
  mbedtls_aes_context aes;
  TRY(mbedtls_aes_setkey_dec(&aes, k, 256));
  TRY(mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, n, iv, s, d));
  return 0;
}

int omemoDriverHkdf(const uint8_t *salt, size_t saltn, const uint8_t *key, size_t keyn, const uint8_t *info, size_t infon, uint8_t *out, size_t outn) {
  TRY(mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, saltn, key, keyn, info, infon, out, outn));
  return 0;
}

int omemoDriverGcmEncrypt(uint8_t *d, const uint8_t key[static 16], size_t n, const uint8_t iv[static 12], uint8_t tag[static 16], const uint8_t *s) {
  int r;
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  if (!(r = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128)))
    r = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, n, iv, 12,
                                  "", 0, s, d, 16, tag);
  mbedtls_gcm_free(&ctx);
  TRY(r);
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
  TRY(r);
  return 0;
}

int omemoDriverCompare(const void *a, const void *b, size_t n) {
  return mbedtls_ct_memcmp(a, b, n);
}
