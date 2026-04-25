#define OMEMO_IMPL
#include "omemo.h"

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/kdf.h>

// TODO: fix error handling

#define TRY(r) do { if ((r) != 1) goto a; } while (0)

int omemoDriverHmac(const omemoKey k, const uint8_t *in, size_t ilen, uint8_t out[static 32]) {
  size_t len;
  int r = OMEMO_ECRYPTO;
  EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
  if (!mac) goto c;
  EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
  if (!ctx) goto b;
  OSSL_PARAM params[] = {
	OSSL_PARAM_construct_utf8_string("digest", "SHA256", 0),
	OSSL_PARAM_END
  };
  TRY(EVP_MAC_init(ctx, k, 32, params));
  TRY(EVP_MAC_update(ctx, in, ilen));
  TRY(EVP_MAC_final(ctx, out, &len, 32));
  TRY(len == 32);
  r = 0;
a:EVP_MAC_CTX_free(ctx);
b:EVP_MAC_free(mac);
c:return r;
}

int omemoDriverAesEncrypt(omemoKey k, size_t n, uint8_t iv[static 16], const uint8_t *s, uint8_t *d) {
  int len, r = OMEMO_ECRYPTO;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) goto b;
  TRY(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, k, iv));
  TRY(EVP_CIPHER_CTX_set_padding(ctx, 0));
  TRY(EVP_EncryptUpdate(ctx, d, &len, s, n));
  TRY(len == n);
  TRY(EVP_EncryptFinal_ex(ctx, d + len, &len));
  TRY(len == 0);
  r = 0;
a:EVP_CIPHER_CTX_free(ctx);
b:return r;
}

int omemoDriverAesDecrypt(omemoKey k, size_t n, uint8_t iv[static 16], const uint8_t *s, uint8_t *d) {
  int len, r = OMEMO_ECRYPTO;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) goto b;
  TRY(EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, k, iv));
  TRY(EVP_CIPHER_CTX_set_padding(ctx, 0));
  TRY(EVP_DecryptUpdate(ctx, d, &len, s, n));
  TRY(len == n);
  TRY(EVP_DecryptFinal_ex(ctx, d + len, &len));
  TRY(len == 0);
  r = 0;
a:EVP_CIPHER_CTX_free(ctx);
b:return r;
}

int omemoDriverHkdf(const uint8_t *salt, size_t saltn, const uint8_t *key, size_t keyn, const uint8_t *info, size_t infon, uint8_t *out, size_t outn) {
  int r = OMEMO_ECRYPTO;
  EVP_KDF *kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
  if (!kdf) goto c;
  EVP_KDF_CTX *kctx = EVP_KDF_CTX_new(kdf);
  if (!kctx) goto b;
  OSSL_PARAM params[] = {
	OSSL_PARAM_construct_utf8_string("digest", "SHA256", 0),
	OSSL_PARAM_construct_octet_string("key",  (void*)key,  keyn),
	OSSL_PARAM_construct_octet_string("salt", (void*)salt, saltn),
	OSSL_PARAM_construct_octet_string("info", (void*)info, infon),
	OSSL_PARAM_END
  };
  TRY(EVP_KDF_derive(kctx, out, outn, params));
  r = 0;
a:EVP_KDF_CTX_free(kctx);
b:EVP_KDF_free(kdf);
c:return r;
}

int omemoDriverGcmEncrypt(uint8_t *d, const uint8_t key[static 16], size_t n, const uint8_t iv[static 12], uint8_t tag[static 16], const uint8_t *s) {
  int len, r = OMEMO_ECRYPTO;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) goto b;
  TRY(EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL));
  TRY(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL));
  TRY(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv));
  TRY(EVP_EncryptUpdate(ctx, d, &len, s, n));
  TRY(len == n);
  TRY(EVP_EncryptFinal_ex(ctx, d + len, &len));
  TRY(len == 0);
  TRY(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, (void*)tag));
  r = 0;
a:EVP_CIPHER_CTX_free(ctx);
b:return r;
}

int omemoDriverGcmDecrypt(uint8_t *d, const uint8_t key[static 16], size_t n, const uint8_t iv[static 12], const uint8_t *tag, size_t tagn, const uint8_t *s) {
  int len, r = OMEMO_ECRYPTO;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) goto b;
  TRY(EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL));
  TRY(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL));
  TRY(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv));
  TRY(EVP_DecryptUpdate(ctx, NULL, &len, "", 0));
  TRY(EVP_DecryptUpdate(ctx, d, &len, s, n));
  TRY(len == n);
  TRY(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tagn, (void*)tag));
  TRY(EVP_DecryptFinal_ex(ctx, d + len, &len));
  TRY(len == 0);
  r = 0;
a:EVP_CIPHER_CTX_free(ctx);
b:return r;
}

int omemoDriverCompare(const void *a, const void *b, size_t n) {
  return CRYPTO_memcmp(a, b, n);
}
