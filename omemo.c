/**
 * Copyright 2024 mierenhoop
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
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef OMEMO_NOHACL
#include "c25519.h"
#else
#include "hacl.h"
#endif

#include "omemo.h"

#ifdef OMEMO2

#define HkdfInfoKeyExchange "OMEMO X3DH"
#define HkdfInfoRootChain   "OMEMO Root Chain"
#define HkdfInfoMessageKeys "OMEMO Message Key Material"
#define HkdfInfoPayload     "OMEMO Payload"

#define PbMsg_n          1
#define PbMsg_pn         2
#define PbMsg_dh_pub     3
#define PbMsg_ciphertext 4

#define PbKeyEx_pk_id   1
#define PbKeyEx_spk_id  2
#define PbKeyEx_ik      3
#define PbKeyEx_ek      4
#define PbKeyEx_message 5

#else

#define HkdfInfoKeyExchange "WhisperText"
#define HkdfInfoRootChain   "WhisperRatchet"
#define HkdfInfoMessageKeys "WhisperMessageKeys"

#define PbMsg_n          2
#define PbMsg_pn         3
#define PbMsg_dh_pub     1
#define PbMsg_ciphertext 4

#define PbKeyEx_pk_id   1
#define PbKeyEx_spk_id  6
#define PbKeyEx_ik      3
#define PbKeyEx_ek      2
#define PbKeyEx_message 4

#endif

#ifdef OMEMO_NOHACL

#define SignModified               edsign_sign_modified
#define MulPackEd                  edsign_sm_pack
#define VerifyEd(sig, pub, msg, n) (!!edsign_verify(sig, pub, msg, n))
#define MapToEd                    morph25519_mx2ey
#define MakeEdKeys                 edsign_sec_to_pub
#define CalcCurve25519(pub, prv)   c25519_smult(pub, c25519_base_x, prv)
#define MapToMont                  morph25519_e2m

#else

#define SignModified               Hacl_Ed25519_sign_modified
#define MulPackEd                  Hacl_Ed25519_pub_from_Curve25519_priv
#define VerifyEd(sig, pub, msg, n) Hacl_Ed25519_verify(pub, n, msg, sig)
#define MapToEd                    Hacl_Curve25519_pub_to_Ed25519_pub
#define MakeEdKeys                 Hacl_Ed25519_seed_to_pub_priv
#define CalcCurve25519(pub, prv)                                       \
  Hacl_Curve25519_51_secret_to_public(pub, prv)
#define MapToMont Hacl_Ed25519_pub_to_Curve25519_pub

#endif

#define TRY(expr)                                                      \
  do {                                                                 \
    int _r_;                                                           \
    if ((_r_ = expr))                                                  \
      return _r_;                                                      \
  } while (0)

enum {
  SESSION_UNINIT = 0,
  SESSION_INIT,
  SESSION_READY,
};

#define SerLen sizeof(omemoSerializedKey)

OMEMO_EXPORT void omemoSerializeKey(omemoSerializedKey k,
                                    const omemoKey pub) {
#ifdef OMEMO2
  memcpy(k, pub, SerLen);
#else
  k[0] = 5;
  memcpy(k + 1, pub, SerLen - 1);
#endif
}

static inline const uint8_t *GetRawKey(const omemoSerializedKey k) {
#ifdef OMEMO2
  return k;
#else
  return k + 1;
#endif
}

/***************************** PROTOBUF ******************************/

// Protobuf: https://protobuf.dev/programming-guides/encoding/

// Only supports uint32 and len prefixed.
struct ProtobufField {
  int type;         // PB_*
  uint32_t v;       // destination varint or LEN
  const uint8_t *p; // LEN element data pointer or NULL
};

#define PB_REQUIRED (1 << 3)
#define PB_UINT32   0
#define PB_LEN      2

/**
 * Parse Protobuf varint.
 *
 * Only supports uint32, higher bits are skipped so it will neither
 * overflow nor clamp to UINT32_MAX.
 *
 * @param s points to the location of the varint in the protobuf data
 * @param e points to the end of the protobuf data
 * @param v (out) points to the location where the parsed varint will be
 * written
 * @returns pointer to first byte after the varint or NULL if parsing is
 * not finished before reaching e
 */
static const uint8_t *ParseVarInt(const uint8_t *s, const uint8_t *e,
                                  uint32_t *v) {
  int i = 0;
  *v = 0;
  do {
    if (s >= e)
      return NULL;
    *v |= (*s & 0x7f) << i;
    i += 7;
  } while (*s++ & 0x80);
  return s;
}

/**
 * Parse data in Protobuf format.
 *
 * For each field encountered it does the following:
 * - Make sure the field number can be stored in `fields` and that the
 *   type corresponds with the one specified in the associated field.
 * - Mark the field number as found which later will be used to check
 *   whether all required fields are found.
 * - Parse the value.
 * - If there already is a non-zero value specified in the field, it is
 *   used to check whether the parsed value is the same.
 * `nfields` should have the value of the highest possible field number
 * + 1. `nfields` must be less than or equal to 16 because we only
 * support a single byte field number, the number is stored like this in
 * the byte: 0nnnnttt where n is the field number and t is the type.
 *
 * @param s is protobuf data
 * @param n is the length of said data
 * @param nfields is the amount of fields in the `fields` array
 * @returns false if successful, true if error
 */
static bool ParseProtobuf(const uint8_t *s, size_t n,
                          struct ProtobufField *fields, int nfields) {
  int type, id;
  uint32_t v;
  const uint8_t *e = s + n;
  uint32_t found = 0;
  assert(nfields <= 16);
  while (s < e) {
    // This is actually a varint, but we only support id < 16 and return
    // an error otherwise, so we don't have to account for multiple-byte
    // tags.
    type = *s & 7;
    id = *s >> 3;
    s++;
    if (id >= nfields || type != (fields[id].type & 7))
      return true;
    found |= 1 << id;
    if (!(s = ParseVarInt(s, e, &v)))
      return true;
    // If field is fixed size, enforce it
    if (fields[id].v && v != fields[id].v)
      return true;
    fields[id].v = v;
    if (type == PB_LEN) {
      fields[id].p = s;
      s += fields[id].v;
    }
  }
  if (s > e)
    return true;
  for (int i = 0; i < nfields; i++) {
    if ((fields[i].type & PB_REQUIRED) && !(found & (1 << i)))
      return true;
  }
  return false;
}

static bool ParseRepeatingField(const uint8_t *s, const uint8_t *e,
                                struct ProtobufField *field,
                                int fieldid) {
  int type, id;
  uint32_t v;
  assert(fieldid <= 16);
  while (s < e) {
    type = *s & 7;
    id = *s >> 3;
    s++;
    if (id >= 16 || (id == fieldid && type != (field->type & 7)))
      return true;
    if (!(s = ParseVarInt(s, e, &v)))
      return true;
    if (id == fieldid)
      field->v = v;
    if (type == PB_LEN) {
      if (id == fieldid)
        field->p = s;
      s += v;
    }
    if (id == fieldid)
      break;
  }
  return s > e;
}

/**
 * Get the size of a properly formatted varint in bytes.
 */
static inline int GetVarIntSize(uint32_t v) {
  return 1 + (v > 0x7f) + (v > 0x3fff) + (v > 0x1fffff) +
         (v > 0xfffffff);
}

static uint8_t *FormatVarInt(uint8_t d[static 6], int type, int id,
                             uint32_t v) {
  assert(id < 16);
  *d++ = (id << 3) | type;
  do {
    *d = v & 0x7f;
    v >>= 7;
    *d++ |= (!!v << 7);
  } while (v);
  return d;
}

#ifndef OMEMO2
static uint8_t *FormatSerializedKey(uint8_t d[static 35], int id,
                                    const omemoKey k) {
  assert(id < 16);
  *d++ = (id << 3) | PB_LEN;
  *d++ = 33;
  omemoSerializeKey(d, k);
  return d + 33;
}
#endif

static uint8_t *FormatKey(uint8_t d[static 34], int id,
                          const omemoKey k) {
  assert(id < 16);
  *d++ = (id << 3) | PB_LEN;
  *d++ = 32;
  memcpy(d, k, 32);
  return d + 32;
}

// Format Protobuf PreKeyWhisperMessage without message (it should be
// appended right after this call).
// This is OMEMOKeyExchange in schema
static size_t FormatPreKeyMessage(
    uint8_t d[static OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE],
    uint32_t pk_id, uint32_t spk_id, const omemoKey ik,
    const omemoKey ek, uint32_t msgsz) {
  uint8_t *p = d;
#ifdef OMEMO2
  p = FormatVarInt(p, PB_UINT32, 1, pk_id);
  p = FormatVarInt(p, PB_UINT32, 2, spk_id);
  p = FormatKey(p, 3, ik);
  p = FormatKey(p, 4, ek);
  // msgsz can be > 127 so we reserve 3 bytes for this
  p = FormatVarInt(p, PB_LEN, 5, msgsz);
#else
  *p++ = (3 << 4) | 3;
  p = FormatVarInt(p, PB_UINT32, 5, 0); // registration id
  p = FormatVarInt(p, PB_UINT32, 1, pk_id);
  p = FormatVarInt(p, PB_UINT32, 6, spk_id);
  p = FormatSerializedKey(p, 3, ik);
  p = FormatSerializedKey(p, 2, ek);
  assert(msgsz < 128);
  p = FormatVarInt(p, PB_LEN, 4, msgsz);
#endif
  return p - d;
}

// Format Protobuf WhisperMessage without ciphertext.
//  HEADER(dh_pair, pn, n)
static size_t
FormatMessageHeader(uint8_t d[static OMEMO_INTERNAL_HEADER_MAXSIZE],
                    uint32_t n, uint32_t pn, const omemoKey dhs,
                    size_t payloadn) {
  uint8_t *p = d;
#ifdef OMEMO2
  p = FormatVarInt(p, PB_UINT32, 1, n);
  p = FormatVarInt(p, PB_UINT32, 2, pn);
  p = FormatKey(p, 3, dhs);
  p = FormatVarInt(p, PB_LEN, 4, payloadn);
#else
  *p++ = (3 << 4) | 3;
  p = FormatSerializedKey(p, 1, dhs);
  p = FormatVarInt(p, PB_UINT32, 2, n);
  p = FormatVarInt(p, PB_UINT32, 3, pn);
  p = FormatVarInt(p, PB_LEN, 4, payloadn);
#endif
  return p - d;
}

/*************************** CRYPTOGRAPHY ****************************/

/**
 * @returns OMEMO_ECORRUPT if the generated shared secret is not secure
 */
static int DoX25519(omemoKey shared, const omemoKey prv,
                    const omemoKey pub) {
#ifdef OMEMO_NOHACL
  c25519_smult(shared, pub, prv);
  return !f25519_eq(shared, f25519_zero) ? 0 : OMEMO_ECORRUPT;
#else
  omemoKey tmp, tmp2;
  memcpy(tmp, prv, 32);
  memcpy(tmp2, pub, 32);
  return Hacl_Curve25519_51_ecdh(shared, tmp, tmp2) ? 0
                                                    : OMEMO_ECORRUPT;
#endif
}

// For OMEMO 0.3, we use the sign_modified as is required.
// For OMEMO >0.3 (OMEMO2 here), the spec describes two options.
// 1: XEdDSA w/ Curve25519 ik
// 2: Any other EdDSA-compatible signature scheme w/ Ed25519 ik
//
// We mix the two: Ed25519 ik with XEdDSA-inspired signatures.
//
// The XEdDSA implementation in libsignal-protocol-c reuses
// sign_modified with code for calculate_key_pair beforehand to convert
// the Curve25519 pub to Ed25519 while handling the sign bit. As we use
// an Ed25519 key internally AND distribute it, thus also not removing
// the sign bit for any party, we can skip the whole generate_key part.
// Essentially the only deviations from regular EdDSA is the
// addition of a randomized nonce to msg and the usage of the hash1(X)
// variation on SHA-512.
static int CalculateCurveSignature(omemoCurveSignature sig,
                                   const struct omemoKeyPair *ik,
                                   const uint8_t rnd[static 64],
                                   const uint8_t *msg, size_t msgn) {
  assert(msgn <= SerLen);
  uint8_t msgbuf[SerLen + 64];
  memcpy(msgbuf, msg, msgn);
  memcpy(msgbuf + msgn, rnd, 64);
  omemoKey ikprv, ikpub;
  memcpy(ikprv, ik->prv, 32);
  memcpy(ikpub, ik->pub, 32);
#ifdef OMEMO2
  SignModified(sig, ikpub, ikprv, msgbuf, msgn);
#else
  omemoKey ed;
  MulPackEd(ed, ikprv);
  int sign = ed[31] & 0x80;
  SignModified(sig, ed, ikprv, msgbuf, msgn);
  sig[63] &= 0x7f;
  sig[63] |= sign;
#endif
  return 0;
}

//  Sig(PK, M)
static bool VerifySignature(const omemoCurveSignature sig,
                            const omemoKey pub, const uint8_t *msg,
                            size_t msgn) {
  assert(msgn <= SerLen);
  uint8_t msgbuf[SerLen];
  memcpy(msgbuf, msg, msgn);
  omemoKey pubcpy;
  memcpy(pubcpy, pub, 32);
  omemoCurveSignature sig2;
  memcpy(sig2, sig, 64);
#ifdef OMEMO2
  return VerifyEd(sig2, pubcpy, msgbuf, msgn);
#else
  omemoKey ed;
  MapToEd(ed, pubcpy);
  ed[31] &= 0x7f;
  ed[31] |= sig[63] & 0x80;
  sig2[63] &= 0x7f;
  return VerifyEd(sig2, ed, msgbuf, msgn);
#endif
}

static int GenerateKeyPair(struct omemoKeyPair *kp) {
  TRY(omemoRandom(kp->prv, sizeof(kp->prv)));
  kp->prv[0] &= 0xf8;
  kp->prv[31] &= 0x7f;
  kp->prv[31] |= 0x40;
  CalcCurve25519(kp->pub, kp->prv);
  return 0;
}

#ifdef OMEMO2
static int GenerateEdKeyPair(struct omemoKeyPair *kp) {
  omemoKey seed;
  TRY(omemoRandom(seed, 32));
  MakeEdKeys(kp->pub, kp->prv, seed);
  return 0;
}
#endif

static int GenerateSignedPreKey(struct omemoSignedPreKey *spk,
                                uint32_t id,
                                const struct omemoKeyPair *idkp) {
  omemoSerializedKey ser;
  spk->id = id;
  TRY(GenerateKeyPair(&spk->kp));
  omemoSerializeKey(ser, spk->kp.pub);
  uint8_t rnd[64];
  TRY(omemoRandom(rnd, 64));
  return CalculateCurveSignature(spk->sig, idkp, rnd, ser, SerLen);
}

/****************************** STORE ********************************/

static inline uint32_t IncrementWrapSkipZero(uint32_t n) {
  n++;
  return n + !n;
}

OMEMO_EXPORT int omemoRefillPreKeys(struct omemoStore *store) {
  if (!store)
    return OMEMO_EPARAM;
  int i;
  for (i = 0; i < OMEMO_NUMPREKEYS; i++) {
    if (!store->prekeys[i].id) {
      struct omemoPreKey pk;
      uint32_t n = IncrementWrapSkipZero(store->pkcounter);
      pk.id = n;
      TRY(GenerateKeyPair(&pk.kp));
      memcpy(store->prekeys + i, &pk, sizeof(struct omemoPreKey));
      store->pkcounter = n;
    }
  }
  return 0;
}

static int omemoSetupStoreImpl(struct omemoStore *store) {
  if (!store)
    return OMEMO_EPARAM;
  memset(store, 0, sizeof(struct omemoStore));
#ifdef OMEMO2
  TRY(GenerateEdKeyPair(&store->identity));
#else
  TRY(GenerateKeyPair(&store->identity));
#endif
  TRY(GenerateSignedPreKey(&store->cursignedprekey, 1,
                           &store->identity));
  TRY(omemoRefillPreKeys(store));
  store->init = true;
  return 0;
}

OMEMO_EXPORT int omemoSetupStore(struct omemoStore *store) {
  if (!store)
    return OMEMO_EPARAM;
  int r;
  if ((r = omemoSetupStoreImpl(store)))
    memset(store, 0, sizeof(struct omemoStore));
  return r;
}

/*********************************************************************/

#define ADSIZE (2 * SerLen)
#ifdef OMEMO2
#define MACSIZE 16
#else
#define MACSIZE 8
#endif

//  AD = Encode(IKA) || Encode(IKB)
static void GetAd(uint8_t ad[static ADSIZE], const omemoKey ika,
                  const omemoKey ikb) {
  omemoSerializeKey(ad, ika);
  omemoSerializeKey(ad + SerLen, ikb);
}

static void Hmac(const omemoKey k, const uint8_t *in, size_t ilen,
                 uint8_t out[static 32]) {
  // Only error return is from parameter verification so we can assert
  assert(!mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                          k, 32, in, ilen, out));
}

static int GetMac(uint8_t d[static MACSIZE], const omemoKey ika,
                  const omemoKey ikb, const omemoKey mk,
                  const uint8_t *msg, size_t msgn) {
  // This could theoretically happen while decrypting when the protobuf
  // is needlessly large.
  if (msgn > OMEMO_INTERNAL_FULLMSG_MAXSIZE + 4)
    return OMEMO_ECORRUPT;
  // Adding 4 in case some client has a large registration id
  uint8_t macinput[ADSIZE + OMEMO_INTERNAL_FULLMSG_MAXSIZE + 4],
      mac[32];
  GetAd(macinput, ika, ikb);
  memcpy(macinput + ADSIZE, msg, msgn);
  Hmac(mk, macinput, ADSIZE + msgn, mac);
  memcpy(d, mac, MACSIZE);
  return 0;
}

static void AesCbc(int mode, uint8_t key[static 32], size_t n,
                   uint8_t iv[static 16], const uint8_t *s,
                   uint8_t *d) {
  // Errors are input validations, so we can assert
  mbedtls_aes_context aes;
  if (mode == MBEDTLS_AES_DECRYPT)
    assert(!mbedtls_aes_setkey_dec(&aes, key, 256));
  else
    assert(!mbedtls_aes_setkey_enc(&aes, key, 256));
  assert(!mbedtls_aes_crypt_cbc(&aes, mode, n, iv, s, d));
}

#define GetPad(n) (16 - ((n) % 16))

static int Encrypt(uint8_t out[OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE],
                   const uint8_t *in, size_t n, omemoKey key,
                   uint8_t iv[static 16]) {
  uint8_t tmp[OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE];
  int pad = GetPad(n);
  memcpy(tmp, in, n);
  memset(tmp + n, pad, pad);
  AesCbc(MBEDTLS_AES_ENCRYPT, key, n + pad, iv, tmp, out);
  return n + pad;
}

static const uint8_t Zero32[32];

#define DeriveKey(salt, secret, info, out)                             \
  (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt,    \
                sizeof(salt), secret, sizeof(secret), info,            \
                sizeof(info) - 1, (uint8_t *)out, sizeof(out))         \
       ? OMEMO_ECRYPTO                                                 \
       : 0)

struct __attribute__((__packed__)) DeriveChainKeyOutput {
  omemoKey cipher, mac;
  uint8_t iv[16];
};

// d may be the same pointer as ck
//  ck, mk = KDF_CK(ck)
static void GetBaseMaterials(omemoKey d, omemoKey mk,
                             const omemoKey ck) {
  uint8_t data[1] = {1};
  Hmac(ck, data, 1, mk);
  data[0] = 2;
  Hmac(ck, data, 1, d);
}

// CKs, mk = KDF_CK(CKs)
// header = HEADER(DHs, PN, Ns)
// Ns += 1
// return header, ENCRYPT(mk, plaintext, CONCAT(AD, header))
static int EncryptKeyImpl(struct omemoSession *session,
                          const struct omemoStore *store,
                          struct omemoKeyMessage *msg,
                          const uint8_t *payload, size_t pn) {
  if (!session->init)
    return OMEMO_ESTATE;
  omemoKey mk;
  GetBaseMaterials(session->state.cks, mk, session->state.cks);
  struct DeriveChainKeyOutput kdfout[1];
  TRY(DeriveKey(Zero32, mk, HkdfInfoMessageKeys, kdfout));
  msg->n = 0;
#ifdef OMEMO2
  msg->p[msg->n++] = (1 << 3) | PB_LEN;
  msg->p[msg->n++] = 16;
  msg->n += 16;
  msg->p[msg->n++] = (2 << 3) | PB_LEN;
  // Hmac'd message will always be smaller than 128
  msg->p[msg->n++] = 0x55; // replaced with actual size
#endif
  msg->n += FormatMessageHeader(
      msg->p + msg->n, session->state.ns, session->state.pn,
      session->state.dhs.pub, pn + GetPad(pn));
  msg->n +=
      Encrypt(msg->p + msg->n, payload, pn, kdfout->cipher, kdfout->iv);
#ifdef OMEMO2
  msg->p[19] = msg->n - 20;
  TRY(GetMac(msg->p + 2, store->identity.pub, session->remoteidentity,
             kdfout->mac, msg->p + 20, msg->n - 20));
#else
  TRY(GetMac(msg->p + msg->n, store->identity.pub,
             session->remoteidentity, kdfout->mac, msg->p, msg->n));
  msg->n += 8;
#endif
  session->state.ns++;
  if (session->init == SESSION_INIT) {
    msg->isprekey = true;
    // [message 00...] -> [00... message] -> [header 00... message] ->
    // [header message]
    memmove(msg->p + OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE, msg->p,
            msg->n);
    int headersz = FormatPreKeyMessage(
        msg->p, session->usedpk_id, session->usedspk_id,
        store->identity.pub, session->usedek, msg->n);
    memmove(msg->p + headersz,
            msg->p + OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE, msg->n);
    msg->n += headersz;
  }
  return 0;
}

OMEMO_EXPORT int omemoEncryptKey(struct omemoSession *session,
                                 const struct omemoStore *store,
                                 struct omemoKeyMessage *msg,
                                 const uint8_t *payload, size_t pn) {
  if (!session || !store || !msg || pn > OMEMO_MAXPAYLOAD)
    return OMEMO_EPARAM;
  int r;
  // Fields outside of session->state are not modified in
  // EncryptKeyImpl() but we'll back them up to save a future headache.
  struct omemoSession backup;
  memcpy(&backup, session, sizeof(struct omemoSession));
  memset(msg, 0, sizeof(struct omemoKeyMessage));
  if ((r = EncryptKeyImpl(session, store, msg, payload, pn))) {
    memcpy(session, &backup, sizeof(struct omemoSession));
    memset(msg, 0, sizeof(struct omemoKeyMessage));
  }
  return r;
}

// RK, ck = KDF_RK(RK, DH(DHs, DHr))
static int DeriveRootKey(struct omemoState *state, omemoKey ck) {
  uint8_t secret[32], masterkey[64];
  TRY(DoX25519(secret, state->dhs.prv, state->dhr));
  TRY(DeriveKey(state->rk, secret, HkdfInfoRootChain, masterkey));
  memcpy(state->rk, masterkey, 32);
  memcpy(ck, masterkey + 32, 32);
  return 0;
}

// DH1 = DH(IKA, SPKB)
// DH2 = DH(EKA, IKB)
// DH3 = DH(EKA, SPKB)
// DH4 = DH(EKA, OPKB)
// SK = KDF(DH1 || DH2 || DH3 || DH4)
static int GetSharedSecret(omemoKey sk, bool isbob, const omemoKey ika,
                           const omemoKey ska, const omemoKey eka,
                           const omemoKey ikb, const omemoKey spkb,
                           const omemoKey opkb) {
  uint8_t secret[32 * 5] = {0}, tmpkey[32];
  memset(secret, 0xff, 32);
  // When we are bob, we must swap the first two.
  TRY(DoX25519(secret + 32, isbob ? ska : ika, isbob ? ikb : spkb));
  TRY(DoX25519(secret + 64, isbob ? ika : ska, isbob ? spkb : ikb));
  TRY(DoX25519(secret + 96, ska, spkb));
  // OMEMO mandates that the bundle MUST contain a prekey.
  TRY(DoX25519(secret + 128, eka, opkb));
  TRY(DeriveKey(Zero32, secret, HkdfInfoKeyExchange, tmpkey));
  memcpy(sk, tmpkey, 32);
  return 0;
}

//  state.DHs = GENERATE_DH()
//  state.DHr = bob_dh_public_key
//  state.RK, state.CKs = KDF_RK(SK, DH(state.DHs, state.DHr))
//  state.CKr = None
//  state.Ns = 0
//  state.Nr = 0
//  state.PN = 0
//  state.MKSKIPPED = {}
static int RatchetInitAlice(struct omemoState *state, const omemoKey sk,
                            const omemoKey ekb,
                            const struct omemoKeyPair *eka) {
  memset(state, 0, sizeof(struct omemoState));
  memcpy(&state->dhs, eka, sizeof(struct omemoKeyPair));
  memcpy(state->rk, sk, 32);
  memcpy(state->dhr, ekb, 32);
  return DeriveRootKey(state, state->cks);
}

OMEMO_EXPORT int omemoInitiateSession(struct omemoSession *session,
                                      const struct omemoStore *store,
                                      const omemoCurveSignature spks,
                                      const omemoSerializedKey spk,
                                      const omemoSerializedKey ik,
                                      const omemoSerializedKey pk,
                                      uint32_t spk_id, uint32_t pk_id) {
  if (!session || !store)
    return OMEMO_EPARAM;
  if (!VerifySignature(spks, GetRawKey(ik), spk, SerLen)) {
    return OMEMO_ECORRUPT;
  }
  struct omemoKeyPair eka;
  TRY(GenerateKeyPair(&eka));
  omemoKey sk;
#ifdef OMEMO2
  omemoKey ikx, edy;
  memcpy(edy, GetRawKey(ik), 32);
  edy[31] &= 0x7f;
  MapToMont(ikx, edy);
  TRY(GetSharedSecret(sk, false, store->identity.prv, eka.prv, eka.prv,
                      ikx, GetRawKey(spk), GetRawKey(pk)));
#else
  TRY(GetSharedSecret(sk, false, store->identity.prv, eka.prv, eka.prv,
                      GetRawKey(ik), GetRawKey(spk), GetRawKey(pk)));
#endif
  int r = RatchetInitAlice(&session->state, sk, GetRawKey(spk), &eka);
  if (r) {
    memset(&session->state, 0, sizeof(struct omemoState));
    return r;
  }
  memcpy(session->usedek, eka.pub, 32);
  memcpy(session->remoteidentity, GetRawKey(ik), 32);
  session->usedpk_id = pk_id;
  session->usedspk_id = spk_id;
  session->init = SESSION_INIT;
  return 0;
}

static const struct omemoPreKey *FindPreKey(const struct omemoStore *store,
                                      uint32_t pk_id) {
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++) {
    if (store->prekeys[i].id == pk_id)
      return store->prekeys + i;
  }
  return NULL;
}

static const struct omemoSignedPreKey *
FindSignedPreKey(const struct omemoStore *store, uint32_t spk_id) {
  if (spk_id == 0)
    return NULL;
  if (store->cursignedprekey.id == spk_id)
    return &store->cursignedprekey;
  if (store->prevsignedprekey.id == spk_id)
    return &store->prevsignedprekey;
  return NULL;
}

OMEMO_EXPORT int omemoRotateSignedPreKey(struct omemoStore *store) {
  if (!store)
    return OMEMO_EPARAM;
  struct omemoSignedPreKey spk;
  int r = GenerateSignedPreKey(
      &spk, IncrementWrapSkipZero(store->cursignedprekey.id),
      &store->identity);
  if (!r) {
    memcpy(&store->prevsignedprekey, &store->cursignedprekey,
           sizeof(struct omemoSignedPreKey));
    memcpy(&store->cursignedprekey, &spk, sizeof(spk));
  }
  return r;
}

//  PN = Ns
//  Ns = 0
//  Nr = 0
//  DHr = dh
//  RK, CKr = KDF_RK(RK, DH(DHs, DHr))
//  DHs = GENERATE_DH()
//  RK, CKs = KDF_RK(RK, DH(DHs, DHr))
static int DHRatchet(struct omemoState *state, const omemoKey dh) {
  state->pn = state->ns;
  state->ns = 0;
  state->nr = 0;
  memcpy(state->dhr, dh, 32);
  TRY(DeriveRootKey(state, state->ckr));
  TRY(GenerateKeyPair(&state->dhs));
  TRY(DeriveRootKey(state, state->cks));
  return 0;
}

static void RatchetInitBob(struct omemoState *state, const omemoKey sk,
                           const struct omemoKeyPair *ekb) {
  memcpy(&state->dhs, ekb, sizeof(struct omemoKeyPair));
  memcpy(state->rk, sk, 32);
}

#define CLAMP0(v) ((v) > 0 ? (v) : 0)

static inline uint32_t GetAmountSkipped(int64_t nr, int64_t n) {
  return CLAMP0(n - nr);
}

static int SkipMessageKeys(struct omemoSession *session, uint32_t n,
                           uint64_t fullamount) {
  struct omemoMessageKey k;
  while (session->state.nr < n) {
    GetBaseMaterials(session->state.ckr, k.mk, session->state.ckr);
    memcpy(k.dh, session->state.dhr, 32);
    k.nr = session->state.nr;
    TRY(omemoStoreMessageKey(session, &k, fullamount--));
    session->state.nr++;
  }
  return 0;
}

static int DecryptKeyImpl(struct omemoSession *session,
                          const struct omemoStore *store,
                          uint8_t *decrypted, size_t *pn,
                          const uint8_t *msg, size_t msgn) {
#ifdef OMEMO2
  struct ProtobufField fields1[3] = {
      [1] = {PB_REQUIRED | PB_LEN, 16}, // mac
      [2] = {PB_REQUIRED | PB_LEN},     // message
  };
  if (ParseProtobuf(msg, msgn, fields1, 3))
    return OMEMO_EPROTOBUF;

  struct ProtobufField fields[5] = {
      [PbMsg_n] = {PB_REQUIRED | PB_UINT32},
      [PbMsg_pn] = {PB_REQUIRED | PB_UINT32},
      [PbMsg_dh_pub] = {PB_REQUIRED | PB_LEN, SerLen},
      [PbMsg_ciphertext] = {PB_REQUIRED | PB_LEN},
  };
  if (ParseProtobuf(fields1[2].p, fields1[2].v, fields, 5))
    return OMEMO_EPROTOBUF;
  const uint8_t *realmac = fields1[1].p;
#else
  if (msgn < 9 || msg[0] != ((3 << 4) | 3))
    return OMEMO_ECORRUPT;
  struct ProtobufField fields[5] = {
      [PbMsg_dh_pub] = {PB_REQUIRED | PB_LEN, SerLen},
      [PbMsg_n] = {PB_REQUIRED | PB_UINT32},
      [PbMsg_pn] = {PB_REQUIRED | PB_UINT32},
      [PbMsg_ciphertext] = {PB_REQUIRED | PB_LEN},
  };
  if (ParseProtobuf(msg + 1, msgn - 9, fields, 5))
    return OMEMO_EPROTOBUF;
  const uint8_t *realmac = msg + msgn - 8;
#endif

  uint32_t encn = fields[PbMsg_ciphertext].v;
  if (encn < 16 || encn % 16 ||
      encn > OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE)
    return OMEMO_ECORRUPT;

  uint32_t headern = fields[PbMsg_n].v;
  uint32_t headerpn = fields[PbMsg_pn].v;
  const uint8_t *headerdh = GetRawKey(fields[PbMsg_dh_pub].p);

  bool shouldstep = !!memcmp(session->state.dhr, headerdh, 32);

  // We first check for maxskip, if that does not pass we should not
  // process the message. If it does pass, we know the total capacity of
  // the array is large enough because c >= maxskip. Then we check if
  // the new keys fit in the remaining space. If that is not the case we
  // return and let the user either remove the old message keys or
  // ignore the message.

  omemoKey mk;
  struct omemoMessageKey key = {0};
  memcpy(key.dh, headerdh, 32);
  key.nr = headern;
  int r;
  if (!(r = omemoLoadMessageKey(session, &key))) {
    memcpy(mk, key.mk, 32);
  } else if (r < 0) {
    return r;
  } else {
    if (!shouldstep && headern < session->state.nr)
      return OMEMO_EKEYGONE;
    uint64_t nskips =
        shouldstep
            ? GetAmountSkipped(session->state.nr, headerpn) + headern
            : GetAmountSkipped(session->state.nr, headern);
    if (shouldstep) {
      TRY(SkipMessageKeys(session, headerpn, nskips));
      nskips -= headern;
      TRY(DHRatchet(&session->state, headerdh));
    }
    TRY(SkipMessageKeys(session, headern, nskips));
    GetBaseMaterials(session->state.ckr, mk, session->state.ckr);
    session->state.nr++;
  }
  struct DeriveChainKeyOutput kdfout[1];
  TRY(DeriveKey(Zero32, mk, HkdfInfoMessageKeys, kdfout));
  uint8_t mac[MACSIZE];
#ifdef OMEMO2
  TRY(GetMac(mac, session->remoteidentity, store->identity.pub,
             kdfout->mac, fields1[2].p, fields1[2].v));
#else
  TRY(GetMac(mac, session->remoteidentity, store->identity.pub,
             kdfout->mac, msg, msgn - 8));
#endif
  if (memcmp(mac, realmac, MACSIZE))
    return OMEMO_ECORRUPT;
  uint8_t tmp[OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE];
  AesCbc(MBEDTLS_AES_DECRYPT, kdfout->cipher, encn, kdfout->iv,
         fields[PbMsg_ciphertext].p, tmp);
  uint8_t pad = tmp[encn - 1];
  if (pad > 16 || pad > encn || encn - pad > *pn)
    return OMEMO_ECORRUPT;
  memcpy(decrypted, tmp, encn - pad);
  *pn = encn - pad;
  session->init = SESSION_READY;
  return 0;
}

static int DecryptGenericKeyImpl(struct omemoSession *session,
                                 const struct omemoStore *store,
                                 uint8_t *payload, size_t *pn,
                                 bool isprekey, const uint8_t *msg,
                                 size_t msgn) {
  const struct omemoPreKey *pk = NULL;
  if (isprekey) {
    // Can't receive prekey when we sent a prekey...
    if (session->init == SESSION_INIT)
      return OMEMO_ESTATE;
#ifdef OMEMO2
    // OMEMOKeyExchange
    struct ProtobufField fields[6] = {
        [PbKeyEx_pk_id] = {PB_REQUIRED | PB_UINT32},
        [PbKeyEx_spk_id] = {PB_REQUIRED | PB_UINT32},
        [PbKeyEx_ik] = {PB_REQUIRED | PB_LEN, SerLen},
        [PbKeyEx_ek] = {PB_REQUIRED | PB_LEN, SerLen},
        [PbKeyEx_message] = {PB_REQUIRED | PB_LEN},
    };
    if (ParseProtobuf(msg, msgn, fields, 6))
      return OMEMO_EPROTOBUF;
#else
    if (msgn == 0 || msg[0] != ((3 << 4) | 3))
      return OMEMO_ECORRUPT;
    // PreKeyWhisperMessage
    struct ProtobufField fields[7] = {
        [5] = {PB_UINT32},                             // registrationid
        [PbKeyEx_pk_id] = {PB_REQUIRED | PB_UINT32},
        [PbKeyEx_spk_id] = {PB_REQUIRED | PB_UINT32},
        [PbKeyEx_ek] = {PB_REQUIRED | PB_LEN, SerLen},
        [PbKeyEx_ik] = {PB_REQUIRED | PB_LEN, SerLen},
        [PbKeyEx_message] = {PB_REQUIRED | PB_LEN},
    };
    if (ParseProtobuf(msg + 1, msgn - 1, fields, 7))
      return OMEMO_EPROTOBUF;
#endif
    if (session->init == SESSION_UNINIT) {
      pk = FindPreKey(store, fields[PbKeyEx_pk_id].v);
      const struct omemoSignedPreKey *spk =
          FindSignedPreKey(store, fields[PbKeyEx_spk_id].v);
      if (!pk || !spk)
        return OMEMO_ECORRUPT;
      session->usedpk_id = fields[PbKeyEx_pk_id].v;
      omemoKey sk;
      memcpy(session->remoteidentity, GetRawKey(fields[PbKeyEx_ik].p),
             32);
#ifdef OMEMO2
      omemoKey ik, edy;
      memcpy(edy, fields[PbKeyEx_ik].p, 32);
      edy[31] &= 0x7f;
      MapToMont(ik, edy);
      TRY(GetSharedSecret(sk, true, store->identity.prv, spk->kp.prv,
                          pk->kp.prv, ik, fields[PbKeyEx_ek].p,
                          fields[PbKeyEx_ek].p));
#else
      TRY(GetSharedSecret(sk, true, store->identity.prv, spk->kp.prv,
                          pk->kp.prv, GetRawKey(fields[PbKeyEx_ik].p),
                          GetRawKey(fields[PbKeyEx_ek].p),
                          GetRawKey(fields[PbKeyEx_ek].p)));
#endif
      RatchetInitBob(&session->state, sk, &spk->kp);
    }
    msg = fields[PbKeyEx_message].p;
    msgn = fields[PbKeyEx_message].v;
  } else if (session->init == SESSION_INIT) {
    // We don't need these anymore
    session->usedpk_id = 0;
    session->usedspk_id = 0;
    memset(session->usedek, 0, 32);
  } else if (session->init == SESSION_UNINIT) {
    return OMEMO_ESTATE;
  }
  return DecryptKeyImpl(session, store, payload, pn, msg, msgn);
}

OMEMO_EXPORT int omemoDecryptKey(struct omemoSession *session,
                                 struct omemoStore *store,
                                 uint8_t *payload, size_t *pn,
                                 bool isprekey, const uint8_t *msg,
                                 size_t msgn) {
  if (!session || !store || !payload || !pn || !store->init || !msg)
    return OMEMO_EPARAM;
  // We only have to backup session->state functionality wise, but to
  // ensure session stays the same before and after an error we backup
  // everything.
  struct omemoSession backup;
  memcpy(&backup, session, sizeof(struct omemoSession));
  int r;
  if ((r = DecryptGenericKeyImpl(session, store, payload, pn, isprekey,
                                 msg, msgn))) {
    memcpy(session, &backup, sizeof(struct omemoSession));
  }
  return r;
}

/******************** MESSAGE CONTENT ENCRYPTION *********************/

#ifdef OMEMO2
OMEMO_EXPORT int omemoDecryptMessage(uint8_t *d, size_t *olen,
                                     const uint8_t *payload, size_t pn,
                                     const uint8_t *s, size_t n) {
  if (!d || !olen || !payload || !s)
    return OMEMO_EPARAM;
  if (pn != 48)
    return OMEMO_ECORRUPT;
  if (n < 16 || n % 16)
    return OMEMO_ECORRUPT;
  uint8_t key[32];
  memcpy(key, payload, 32);
  struct DeriveChainKeyOutput kdfout[1];
  TRY(DeriveKey(Zero32, key, HkdfInfoPayload, kdfout));
  uint8_t mac[32];
  Hmac(kdfout->mac, s, n, mac);
  if (memcmp(mac, payload + 32, 16))
    return OMEMO_ECORRUPT;
  AesCbc(MBEDTLS_AES_DECRYPT, kdfout->cipher, n, kdfout->iv, s, d);

  uint8_t p = d[n - 1];
  if (p > n)
    return OMEMO_ECORRUPT;
  memset(d - p, 0, p);
  *olen = n - p;
  return 0;
}
#else
OMEMO_EXPORT int omemoDecryptMessage(uint8_t *d, const uint8_t *payload,
                                     size_t pn, const uint8_t iv[12],
                                     const uint8_t *s, size_t n) {
  if (!d || !payload || !iv || !s)
    return OMEMO_EPARAM;
  int r = 0;
  if (pn < 32)
    return OMEMO_ECORRUPT;
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  if (!(r = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, payload,
                               128)))
    r = mbedtls_gcm_auth_decrypt(&ctx, n, iv, 12, "", 0, payload + 16,
                                 pn - 16, s, d);
  mbedtls_gcm_free(&ctx);
  return r ? OMEMO_ECRYPTO : 0;
}
#endif

#ifdef OMEMO2
OMEMO_EXPORT int omemoEncryptMessage(uint8_t *d, uint8_t payload[48],
                                     uint8_t *s, size_t n) {
  if (!d || !payload || !s)
    return OMEMO_EPARAM;
  uint8_t key[32];
  TRY(omemoRandom(key, 32));
  struct DeriveChainKeyOutput kdfout[1];
  TRY(DeriveKey(Zero32, key, HkdfInfoPayload, kdfout));
  // PKCS#7
  size_t extend = omemoGetMessagePadSize(n);
  memset(s + n, extend, extend);
  AesCbc(MBEDTLS_AES_ENCRYPT, kdfout->cipher, n + extend, kdfout->iv, s,
         d);
  uint8_t mac[32];
  Hmac(kdfout->mac, d, n + extend, mac);
  memcpy(payload, key, 32);
  memcpy(payload + 32, mac, 16);
  return 0;
}
#else
OMEMO_EXPORT int omemoEncryptMessage(uint8_t *d, uint8_t payload[32],
                                     uint8_t iv[12], const uint8_t *s,
                                     size_t n) {
  if (!d || !payload || !iv || !s)
    return OMEMO_EPARAM;
  int r = 0;
  if ((r = omemoRandom(payload, 16)) || (r = omemoRandom(iv, 12)))
    return r;
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  if (!(r = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, payload,
                               128)))
    r = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, n, iv, 12,
                                  "", 0, s, d, 16, payload + 16);
  mbedtls_gcm_free(&ctx);
  return r ? OMEMO_ECRYPTO : 0;
}
#endif

/************************** SERIALIZATION ****************************/

size_t omemoGetSerializedStoreSize(const struct omemoStore *store) {
  if (!store)
    return 0;
  size_t sum = 34 * 6 + (2 + 64) * 2 + 1 * 4 +
               GetVarIntSize(store->init) +
               GetVarIntSize(store->cursignedprekey.id) +
               GetVarIntSize(store->prevsignedprekey.id) +
               GetVarIntSize(store->pkcounter);
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++)
    sum += 2 + 1 + GetVarIntSize(store->prekeys[i].id) + 2 * 34;
  return sum;
}

OMEMO_EXPORT void omemoSerializeStore(uint8_t *p,
                                      const struct omemoStore *store) {
  if (!p || !store)
    return;
  uint8_t *d = p;
  d = FormatVarInt(d, PB_UINT32, 1, store->init);
  d = FormatKey(d, 2, store->identity.prv);
  d = FormatKey(d, 3, store->identity.pub);
  d = FormatVarInt(d, PB_UINT32, 4, store->cursignedprekey.id);
  d = FormatKey(d, 5, store->cursignedprekey.kp.prv);
  d = FormatKey(d, 6, store->cursignedprekey.kp.pub);
  d = FormatVarInt(d, PB_LEN, 7, 64);
  d = (memcpy(d, store->cursignedprekey.sig, 64), d + 64);
  d = FormatVarInt(d, PB_UINT32, 8, store->prevsignedprekey.id);
  d = FormatKey(d, 9, store->prevsignedprekey.kp.prv);
  d = FormatKey(d, 10, store->prevsignedprekey.kp.pub);
  d = FormatVarInt(d, PB_LEN, 11, 64);
  d = (memcpy(d, store->prevsignedprekey.sig, 64), d + 64);
  d = FormatVarInt(d, PB_UINT32, 12, store->pkcounter);
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++) {
    const struct omemoPreKey *pk = store->prekeys + i;
    d = FormatVarInt(d, PB_LEN, 13, 1 + GetVarIntSize(pk->id) + 2 * 34);
    d = FormatVarInt(d, PB_UINT32, 1, pk->id);
    d = FormatKey(d, 2, pk->kp.prv);
    d = FormatKey(d, 3, pk->kp.pub);
  }
  assert(d - p == omemoGetSerializedStoreSize(store));
}

OMEMO_EXPORT int omemoDeserializeStore(const char *p, size_t n,
                                       struct omemoStore *store) {
  if (!p || !store)
    return OMEMO_EPARAM;
  struct ProtobufField fields[] = {
      [1] = {PB_REQUIRED | PB_UINT32},
      [2] = {PB_REQUIRED | PB_LEN, 32},
      [3] = {PB_REQUIRED | PB_LEN, 32},
      [4] = {PB_REQUIRED | PB_UINT32},
      [5] = {PB_REQUIRED | PB_LEN, 32},
      [6] = {PB_REQUIRED | PB_LEN, 32},
      [7] = {PB_REQUIRED | PB_LEN, 64},
      [8] = {PB_REQUIRED | PB_UINT32},
      [9] = {PB_REQUIRED | PB_LEN, 32},
      [10] = {PB_REQUIRED | PB_LEN, 32},
      [11] = {PB_REQUIRED | PB_LEN, 64},
      [12] = {PB_REQUIRED | PB_UINT32},
      [13] = {/*PB_REQUIRED |*/ PB_LEN},
  };
  if (ParseProtobuf(p, n, fields, 14))
    return OMEMO_EPROTOBUF;
  store->init = fields[1].v;
  memcpy(store->identity.prv, fields[2].p, 32);
  memcpy(store->identity.pub, fields[3].p, 32);
  store->cursignedprekey.id = fields[4].v;
  memcpy(store->cursignedprekey.kp.prv, fields[5].p, 32);
  memcpy(store->cursignedprekey.kp.pub, fields[6].p, 32);
  memcpy(store->cursignedprekey.sig, fields[7].p, 64);
  store->prevsignedprekey.id = fields[8].v;
  memcpy(store->prevsignedprekey.kp.prv, fields[9].p, 32);
  memcpy(store->prevsignedprekey.kp.pub, fields[10].p, 32);
  memcpy(store->prevsignedprekey.sig, fields[11].p, 64);
  store->pkcounter = fields[12].v;
  const char *e = p + n;
  int i = 0;
  while (i < OMEMO_NUMPREKEYS &&
         !ParseRepeatingField(p, e, &fields[13], 13) && fields[13].p) {
    struct ProtobufField innerfields[] = {
        [1] = {PB_REQUIRED | PB_UINT32},
        [2] = {PB_REQUIRED | PB_LEN, 32},
        [3] = {PB_REQUIRED | PB_LEN, 32},
    };
    if (ParseProtobuf(fields[13].p, fields[13].v, innerfields, 4))
      return OMEMO_EPROTOBUF;
    store->prekeys[i].id = innerfields[1].v;
    memcpy(store->prekeys[i].kp.prv, innerfields[2].p, 32);
    memcpy(store->prekeys[i].kp.pub, innerfields[3].p, 32);
    i++;
    p = fields[13].p + fields[13].v;
    fields[13].v = 0, fields[13].p = NULL;
  }
  return 0;
}

size_t
omemoGetSerializedSessionSize(const struct omemoSession *session) {
  if (!session)
    return 0;
  return 34 * 8 // Key
         + 1 * 6 + GetVarIntSize(session->state.ns) +
         GetVarIntSize(session->state.nr) +
         GetVarIntSize(session->state.pn) +
         GetVarIntSize(session->usedpk_id) +
         GetVarIntSize(session->usedspk_id) +
         GetVarIntSize(session->init);
}

OMEMO_EXPORT void
omemoSerializeSession(uint8_t *p, const struct omemoSession *session) {
  if (!p || !session)
    return;
  uint8_t *d = p;
  d = FormatKey(d, 1, session->remoteidentity);
  d = FormatKey(d, 2, session->state.dhs.prv);
  d = FormatKey(d, 3, session->state.dhs.pub);
  d = FormatKey(d, 4, session->state.dhr);
  d = FormatKey(d, 5, session->state.rk);
  d = FormatKey(d, 6, session->state.cks);
  d = FormatKey(d, 7, session->state.ckr);
  d = FormatVarInt(d, PB_UINT32, 8, session->state.ns);
  d = FormatVarInt(d, PB_UINT32, 9, session->state.nr);
  d = FormatVarInt(d, PB_UINT32, 10, session->state.pn);
  // TODO: don't have to include used* after first ratchet
  d = FormatKey(d, 11, session->usedek);
  d = FormatVarInt(d, PB_UINT32, 12, session->usedpk_id);
  d = FormatVarInt(d, PB_UINT32, 13, session->usedspk_id);
  d = FormatVarInt(d, PB_UINT32, 14, session->init);
  assert(d - p == omemoGetSerializedSessionSize(session));
}

OMEMO_EXPORT int omemoDeserializeSession(const char *p, size_t n,
                                         struct omemoSession *session) {
  if (!p || !session)
    return OMEMO_EPARAM;
  struct ProtobufField fields[] = {
      [1] = {PB_REQUIRED | PB_LEN, 32},
      [2] = {PB_REQUIRED | PB_LEN, 32},
      [3] = {PB_REQUIRED | PB_LEN, 32},
      [4] = {PB_REQUIRED | PB_LEN, 32},
      [5] = {PB_REQUIRED | PB_LEN, 32},
      [6] = {PB_REQUIRED | PB_LEN, 32},
      [7] = {PB_REQUIRED | PB_LEN, 32},
      [8] = {PB_REQUIRED | PB_UINT32},
      [9] = {PB_REQUIRED | PB_UINT32},
      [10] = {PB_REQUIRED | PB_UINT32},
      [11] = {PB_REQUIRED | PB_LEN, 32},
      [12] = {PB_REQUIRED | PB_UINT32},
      [13] = {PB_REQUIRED | PB_UINT32},
      [14] = {PB_REQUIRED | PB_UINT32},
  };
  if (ParseProtobuf(p, n, fields, 15))
    return OMEMO_EPROTOBUF;
  memcpy(session->remoteidentity, fields[1].p, 32);
  memcpy(session->state.dhs.prv, fields[2].p, 32);
  memcpy(session->state.dhs.pub, fields[3].p, 32);
  memcpy(session->state.dhr, fields[4].p, 32);
  memcpy(session->state.rk, fields[5].p, 32);
  memcpy(session->state.cks, fields[6].p, 32);
  memcpy(session->state.ckr, fields[7].p, 32);
  session->state.ns = fields[8].v;
  session->state.nr = fields[9].v;
  session->state.pn = fields[10].v;
  memcpy(session->usedek, fields[11].p, 32);
  session->usedpk_id = fields[12].v;
  session->usedspk_id = fields[13].v;
  session->init = fields[14].v;
  return 0;
}
