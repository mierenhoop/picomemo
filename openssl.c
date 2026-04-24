#include "omemo.h"

#include <openssl/evp.h>
#include <openssl/kdf.h>

// TODO: fix error handling

int omemoDriverHmac(const omemoKey k, const uint8_t *in, size_t ilen, uint8_t out[static 32]) {
  size_t out_len;
  EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
  EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
  OSSL_PARAM params[] = {
	OSSL_PARAM_construct_utf8_string("digest", "SHA256", 0),
	OSSL_PARAM_END
  };
  EVP_MAC_init(ctx, k, 32, params);
  EVP_MAC_update(ctx, in, ilen);
  EVP_MAC_final(ctx, out, &out_len, 32);
  EVP_MAC_CTX_free(ctx);
  EVP_MAC_free(mac);
  return 0;
}

int omemoDriverAesEncrypt(omemoKey k, size_t n, uint8_t iv[static 16], const uint8_t *s, uint8_t *d) {
  int len, final_len;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, k, iv);
  EVP_EncryptUpdate(ctx, d, &len, s, n);
  EVP_EncryptFinal_ex(ctx, d + len, &final_len);
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

int omemoDriverAesDecrypt(omemoKey k, size_t n, uint8_t iv[static 16], const uint8_t *s, uint8_t *d) {
  int len, final_len;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, k, iv);
  EVP_DecryptUpdate(ctx, d, &len, s, n);
  EVP_DecryptFinal_ex(ctx, d + len, &final_len);
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

int omemoDriverHkdf(const uint8_t *salt, size_t saltn, const uint8_t *key, size_t keyn, const uint8_t *info, size_t infon, uint8_t *out, size_t outn) {
  EVP_KDF *kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
  EVP_KDF_CTX *kctx = EVP_KDF_CTX_new(kdf);
  OSSL_PARAM params[] = {
	OSSL_PARAM_construct_utf8_string("digest", "SHA256", 0),
	OSSL_PARAM_construct_octet_string("key",  (void*)key,  keyn),
	OSSL_PARAM_construct_octet_string("salt", (void*)salt, saltn),
	OSSL_PARAM_construct_octet_string("info", (void*)info, infon),
	OSSL_PARAM_END
  };
  EVP_KDF_derive(kctx, out, outn, params);
  EVP_KDF_CTX_free(kctx);
  EVP_KDF_free(kdf);
  return 0;
}

int omemoDriverGcmDecrypt(uint8_t *d, const uint8_t key[static 16], size_t n, const uint8_t iv[static 12], const uint8_t *tag, size_t tagn, const uint8_t *s) {
  int len, final_len;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL);
  EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv);
  EVP_DecryptUpdate(ctx, NULL, &len, "", 0);
  EVP_DecryptUpdate(ctx, d, &len, s, n);
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tagn, (void*)tag);
  int _r = EVP_DecryptFinal_ex(ctx, d + len, &final_len);
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}
