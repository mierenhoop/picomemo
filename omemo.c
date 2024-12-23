/**
 * Copyright 2024 mierenhoop
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <mbedtls/hkdf.h>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <setjmp.h>

#include <sys/random.h>

#include "c25519.h"

#include "omemo.h"

typedef struct Ctx {
  jmp_buf jb;
} *CTX;

#define SetupCtx(ctx)               \
  struct Ctx _setupctx_##ctx = {0}; \
  CTX ctx = &_setupctx_##ctx;

#define Recover(ctx, r) (r = setjmp((ctx)->jb))
#define Throw(ctx, r) longjmp((ctx)->jb, r)

enum {
  SESSION_UNINIT = 0,
  SESSION_INIT,
  SESSION_READY,
};

static void GetRandom(CTX ctx, void *p, size_t n) {
  if (omemoRandom(p, n))
    Throw(ctx, OMEMO_ECRYPTO);
}

// Protobuf: https://protobuf.dev/programming-guides/encoding/

// Only supports uint32 and len prefixed (by int32).
struct ProtobufField {
  int type; // PB_*
  uint32_t v; // destination varint or LEN
  const uint8_t *p; // LEN element data pointer or NULL
};

#define PB_REQUIRED (1 << 3)
#define PB_UINT32 0
#define PB_LEN 2

/**
 * Parse Protobuf varint.
 *
 * Only supports uint32, higher bits are skipped so it will neither overflow nor clamp to UINT32_MAX.
 *
 * @param s points to the location of the varint in the protobuf data
 * @param e points to the end of the protobuf data
 * @param v (out) points to the location where the parsed varint will be
 * written
 * @returns pointer to first byte after the varint or NULL if parsing is
 * not finished before reaching e
 */
static const uint8_t *ParseVarInt(const uint8_t *s, const uint8_t *e, uint32_t *v) {
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

/**
 * Get the size of a properly formatted varint in bytes.
 */
static inline int GetVarIntSize(uint32_t v) {
  return 1 + (v > 0x7f) + (v > 0x3fff) + (v > 0x1fffff) +
         (v > 0xfffffff);
}

static uint8_t *FormatVarInt(uint8_t d[static 6], int type, int id, uint32_t v) {
  assert(id < 16);
  *d++ = (id << 3) | type;
  do {
    *d = v & 0x7f;
    v >>= 7;
    *d++ |= (!!v << 7);
  } while (v);
  return d;
}

void omemoSerializeKey(omemoSerializedKey k, const omemoKey pub) {
  k[0] = 5;
  memcpy(k + 1, pub, sizeof(omemoSerializedKey) - 1);
}

static uint8_t *FormatKey(uint8_t d[35], int id, const omemoKey k) {
  assert(id < 16);
  *d++ = (id << 3) | PB_LEN;
  *d++ = 33;
  omemoSerializeKey(d, k);
  return d + 33;
}

static uint8_t *FormatPrivateKey(uint8_t d[34], int id, const omemoKey k) {
  assert(id < 16);
  *d++ = (id << 3) | PB_LEN;
  *d++ = 32;
  memcpy(d, k, 32);
  return d + 32;
}

// Format Protobuf PreKeyWhisperMessage without message (it should be
// appended right after this call).
static size_t FormatPreKeyMessage(uint8_t d[OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE],
                                  uint32_t pk_id, uint32_t spk_id,
                                  const omemoKey ik, const omemoKey ek,
                                  uint32_t msgsz) {
  assert(msgsz < 128);
  uint8_t *p = d;
  *p++ = (3 << 4) | 3;
  p = FormatVarInt(p, PB_UINT32, 5, 0xcc); // TODO: registration id
  p = FormatVarInt(p, PB_UINT32, 1, pk_id);
  p = FormatVarInt(p, PB_UINT32, 6, spk_id);
  p = FormatKey(p, 3, ik);
  p = FormatKey(p, 2, ek);
  assert(msgsz <= 0x7f);
  p = FormatVarInt(p, PB_LEN, 4, msgsz);
  return p - d;
}

// Format Protobuf WhisperMessage without ciphertext.
//  HEADER(dh_pair, pn, n)
static size_t FormatMessageHeader(uint8_t d[OMEMO_INTERNAL_HEADER_MAXSIZE], uint32_t n,
                                  uint32_t pn, const omemoKey dhs) {
  uint8_t *p = d;
  *p++ = (3 << 4) | 3;
  p = FormatKey(p, 1, dhs);
  p = FormatVarInt(p, PB_UINT32, 2, n);
  p = FormatVarInt(p, PB_UINT32, 3, pn);
  *p++ = (4 << 3) | PB_LEN;
  *p++ = OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE;
  return p - d;
}

// TODO: remove
/**
 * Remove the skipped message key that has just been used for
 * decrypting.
 *
 *  del state.MKSKIPPED[header.dh, header.n]
 */
static void
NormalizeSkipMessageKeysTrivial(struct omemoSkippedMessageKeys *s) {
  assert(s->p && s->n <= s->c);
  if (s->removed) {
    assert(s->p <= s->removed && s->removed < s->p + s->n);
    size_t n = s->n - (s->removed - s->p) - 1;
    memmove(s->removed, s->removed + 1,
            n * sizeof(struct omemoSkippedMessageKeys));
    s->n--;
    s->removed = NULL;
  }
}

static void ConvertCurvePrvToEdPub(omemoKey ed, const omemoKey prv) {
  struct ed25519_pt p;
  ed25519_smult(&p, &ed25519_base, prv);
  uint8_t x[F25519_SIZE];
  uint8_t y[F25519_SIZE];
  ed25519_unproject(x, y, &p);
  ed25519_pack(ed, x, y);
}

static void c25519_sign(CTX ctx, omemoCurveSignature sig, const omemoKey prv, const uint8_t *msg, size_t msgn) {
  assert(msgn <= 33);
  omemoKey ed;
  uint8_t msgbuf[33+64];
  int sign = 0;
  memcpy(msgbuf, msg, msgn);
  GetRandom(ctx, msgbuf+msgn, 64);
  ConvertCurvePrvToEdPub(ed, prv);
  sign = ed[31] & 0x80;
  edsign_sign_modified(sig, ed, prv, msgbuf, msgn);
  sig[63] &= 0x7f;
  sig[63] |= sign;
}

static bool c25519_verify(const omemoCurveSignature sig, const omemoKey pub, const uint8_t *msg, size_t msgn) {
  omemoKey ed;
  morph25519_mx2ey(ed, pub);
  ed[31] &= 0x7f;
  ed[31] |= sig[63] & 0x80;
  omemoCurveSignature sig2;
  memcpy(sig2, sig, 64);
  sig2[63] &= 0x7f;
  return !!edsign_verify(sig2, ed, msg, msgn);
}

static void GenerateKeyPair(CTX ctx, struct omemoKeyPair *kp) {
  memset(kp, 0, sizeof(*kp));
  GetRandom(ctx, kp->prv, sizeof(kp->prv));
  c25519_prepare(kp->prv);
  curve25519(kp->pub, kp->prv, c25519_base_x);
}

static void GeneratePreKey(CTX ctx, struct omemoPreKey *pk, uint32_t id) {
  pk->id = id;
  GenerateKeyPair(ctx, &pk->kp);
}

static void GenerateIdentityKeyPair(CTX ctx, struct omemoKeyPair *kp) {
  GenerateKeyPair(ctx, kp);
}

static void GenerateRegistrationId(CTX ctx, uint32_t *id) {
  GetRandom(ctx, id, sizeof(*id));
  *id = (*id % 16380) + 1;
}

static void CalculateCurveSignature(CTX ctx, omemoCurveSignature sig, omemoKey signprv,
                                    uint8_t *msg, size_t n) {
  assert(n <= 33);
  uint8_t rnd[sizeof(omemoCurveSignature)], buf[33 + 128];
  GetRandom(ctx, rnd, sizeof(rnd));
  c25519_sign(ctx, sig, signprv, msg, n);
}

//  DH(dh_pair, dh_pub)
static void CalculateCurveAgreement(uint8_t d[static 32], const omemoKey prv,
                                    const omemoKey pub) {

  curve25519(d, prv, pub);
}

static void GenerateSignedPreKey(CTX ctx, struct omemoSignedPreKey *spk,
                                 uint32_t id,
                                 struct omemoKeyPair *idkp) {
  omemoSerializedKey ser;
  spk->id = id;
  GenerateKeyPair(ctx, &spk->kp);
  omemoSerializeKey(ser, spk->kp.pub);
  CalculateCurveSignature(ctx, spk->sig, idkp->prv, ser,
                          sizeof(omemoSerializedKey));
}

//  Sig(PK, M)
static bool VerifySignature(const omemoCurveSignature sig, const omemoKey sk,
                            const uint8_t *msg, size_t n) {
  return c25519_verify(sig, sk, msg, n);
}

static inline uint32_t IncrementWrapSkipZero(uint32_t n) {
  n++;
  return n + !n;
}

int omemoRefillPreKeys(struct omemoStore *store) {
  int i, r;
  SetupCtx(ctx);
  if (Recover(ctx, r))
    return r;
  for (i = 0; i < OMEMO_NUMPREKEYS; i++) {
    if (!store->prekeys[i].id) {
      struct omemoPreKey pk;
      uint32_t n = IncrementWrapSkipZero(store->pkcounter);
      GeneratePreKey(ctx, &pk, n);
      memcpy(store->prekeys+i, &pk, sizeof(struct omemoPreKey));
      store->pkcounter = n;
    }
  }
  return 0;
}

int omemoSetupStore(struct omemoStore *store) {
  int r;
  SetupCtx(ctx);
  if (Recover(ctx, r)) {
    memset(store, 0, sizeof(struct omemoStore));
    return r;
  }
  memset(store, 0, sizeof(struct omemoStore));
  GenerateIdentityKeyPair(ctx, &store->identity);
  GenerateSignedPreKey(ctx, &store->cursignedprekey, 1, &store->identity);
  if ((r = omemoRefillPreKeys(store)))
    Throw(ctx, r);
  store->isinitialized = true;
  return 0;
}

// TODO: REMOVE
int omemoSetupSession(struct omemoSession *session, size_t cap) {
  memset(session, 0, sizeof(struct omemoSession));
  if (!(session->mkskipped.p = malloc(cap * sizeof(struct omemoMessageKey)))) {
    return OMEMO_EALLOC;
  }
  session->mkskipped.c = cap;
  return 0;
}

// TODO: remove
void omemoFreeSession(struct omemoSession *session) {
  if (session->mkskipped.p) {
    free(session->mkskipped.p);
    session->mkskipped.p = NULL;
  }
}

//  AD = Encode(IKA) || Encode(IKB)
static void GetAd(uint8_t ad[static 66], const omemoKey ika, const omemoKey ikb) {
  omemoSerializeKey(ad, ika);
  omemoSerializeKey(ad + 33, ikb);
}

static void GetMac(CTX ctx, uint8_t d[static 8], const omemoKey ika, const omemoKey ikb,
                  const omemoKey mk, const uint8_t *msg, size_t msgn) {
  assert(msgn <= OMEMO_INTERNAL_FULLMSG_MAXSIZE);
  uint8_t macinput[66 + OMEMO_INTERNAL_FULLMSG_MAXSIZE], mac[32];
  GetAd(macinput, ika, ikb);
  memcpy(macinput + 66, msg, msgn);
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), mk,
                      32, macinput, 66 + msgn, mac) != 0)
    Throw(ctx, OMEMO_ECRYPTO);
  memcpy(d, mac, 8);
}

static bool VerifyMac(CTX ctx, const omemoKey ika, const omemoKey ikb,
                  struct omemoKeyDecryptor *dec) {
  uint8_t mac[32];
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), dec->mk,
                      32, dec->macinput, dec->maclen, mac) != 0)
    Throw(ctx, OMEMO_ECRYPTO);
  return !!memcmp(mac, dec->mac, 8);
}

static void Encrypt(CTX ctx, uint8_t out[OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE], const omemoKeyPayload in, omemoKey key,
                    uint8_t iv[static 16]) {
  assert(OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE == 48);
  uint8_t tmp[48];
  memcpy(tmp, in, 32);
  memset(tmp+32, 0x10, 0x10);
  mbedtls_aes_context aes;
  if (mbedtls_aes_setkey_enc(&aes, key, 256)
   || mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, 48,
                               iv, tmp, out))
    Throw(ctx, OMEMO_ECRYPTO);
}

static void Decrypt(CTX ctx, uint8_t *out, const uint8_t *in, size_t n, omemoKey key,
                    uint8_t iv[static 16]) {
  mbedtls_aes_context aes;
  if (mbedtls_aes_setkey_dec(&aes, key, 256) ||
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, n,
                               iv, in, out))
    Throw(ctx, OMEMO_ECRYPTO);
}

struct __attribute__((__packed__)) DeriveChainKeyOutput {
  omemoKey cipher, mac;
  uint8_t iv[16];
};

static void DeriveChainKey(CTX ctx, struct DeriveChainKeyOutput *out, const omemoKey ck) {
  uint8_t salt[32];
  memset(salt, 0, 32);
  assert(sizeof(struct DeriveChainKeyOutput) == 80);
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      salt, 32, ck, 32, "WhisperMessageKeys",
                      18, (uint8_t *)out,
                      sizeof(struct DeriveChainKeyOutput)))
    Throw(ctx, OMEMO_ECRYPTO);
}

// d may be the same pointer as ck
//  ck, mk = KDF_CK(ck)
static void GetBaseMaterials(CTX ctx, omemoKey d, omemoKey mk, const omemoKey ck) {
  omemoKey tmp;
  uint8_t data = 1;
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), ck, 32, &data, 1, mk) != 0)
    Throw(ctx, OMEMO_ECRYPTO);
  data = 2;
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), ck, 32, &data, 1, tmp) != 0)
    Throw(ctx, OMEMO_ECRYPTO);
  memcpy(d, tmp, 32);
}

// CKs, mk = KDF_CK(CKs)
// header = HEADER(DHs, PN, Ns)
// Ns += 1
// return header, ENCRYPT(mk, plaintext, CONCAT(AD, header))
static void EncryptKeyImpl(CTX ctx, struct omemoSession *session, const struct omemoStore *store, struct omemoKeyMessage *msg, const omemoKeyPayload payload) {
  if (session->fsm != SESSION_INIT && session->fsm != SESSION_READY)
    Throw(ctx, OMEMO_ESTATE);
  omemoKey mk;
  GetBaseMaterials(ctx, session->state.cks, mk, session->state.cks);
  struct DeriveChainKeyOutput kdfout;
  DeriveChainKey(ctx, &kdfout, mk);
  msg->n = FormatMessageHeader(msg->p, session->state.ns, session->state.pn, session->state.dhs.pub);
  Encrypt(ctx, msg->p+msg->n, payload, kdfout.cipher, kdfout.iv);
  msg->n += OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE;
  GetMac(ctx, msg->p+msg->n, store->identity.pub, session->remoteidentity, kdfout.mac, msg->p, msg->n);
  msg->n += 8;
  session->state.ns++;
  if (session->fsm == SESSION_INIT) {
    msg->isprekey = true;
    // [message 00...] -> [00... message] -> [header 00... message] ->
    // [header message]
    memmove(msg->p + OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE, msg->p, msg->n);
    int headersz =
        FormatPreKeyMessage(msg->p, session->pendingpk_id, session->pendingspk_id,
                            store->identity.pub, session->pendingek, msg->n);
    memmove(msg->p + headersz, msg->p + OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE, msg->n);
    msg->n += headersz;
  }
}

int omemoEncryptKey(struct omemoSession *session, const struct omemoStore *store, struct omemoKeyMessage *msg, const omemoKeyPayload payload) {
  int r;
  struct omemoState backup;
  memcpy(&backup, &session->state, sizeof(struct omemoState));
  memset(msg, 0, sizeof(struct omemoKeyMessage));
  SetupCtx(ctx);
  if (Recover(ctx, r)) {
    memcpy(&session->state, &backup, sizeof(struct omemoState));
    memset(msg, 0, sizeof(struct omemoKeyMessage));
  }
  EncryptKeyImpl(ctx, session, store, msg, payload);
  return r;
}

// RK, ck = KDF_RK(RK, DH(DHs, DHr))
static void DeriveRootKey(CTX ctx, struct omemoState *state, omemoKey ck) {
  uint8_t secret[32], masterkey[64];
  CalculateCurveAgreement(secret, state->dhs.prv, state->dhr);
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      state->rk, 32, secret, sizeof(secret),
                      "WhisperRatchet", 14, masterkey, 64) != 0)
    Throw(ctx, OMEMO_ECRYPTO);
  memcpy(state->rk, masterkey, 32);
  memcpy(ck, masterkey + 32, 32);
}


// DH1 = DH(IKA, SPKB)
// DH2 = DH(EKA, IKB)
// DH3 = DH(EKA, SPKB)
// DH4 = DH(EKA, OPKB)
// SK = KDF(DH1 || DH2 || DH3 || DH4)
static void GetSharedSecret(CTX ctx, omemoKey sk, bool isbob, const omemoKey ika, const omemoKey ska, const omemoKey eka, const omemoKey ikb, const omemoKey spkb, const omemoKey opkb) {
  uint8_t secret[32*5] = {0}, salt[32];
  memset(secret, 0xff, 32);
  // When we are bob, we must swap the first two.
  CalculateCurveAgreement(secret+32, isbob ? ska : ika, isbob ? ikb : spkb);
  CalculateCurveAgreement(secret+64, isbob ? ika : ska, isbob ? spkb : ikb);
  CalculateCurveAgreement(secret+96, ska, spkb);
  // OMEMO mandates that the bundle MUST contain a prekey.
  CalculateCurveAgreement(secret+128, eka, opkb);
  memset(salt, 0, 32);
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, 32, secret, sizeof(secret), "WhisperText", 11, sk, 32) != 0)
    Throw(ctx, OMEMO_ECRYPTO);
  uint8_t full[64];
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, 32, secret, sizeof(secret), "WhisperText", 11, full, 64) != 0)
    Throw(ctx, OMEMO_ECRYPTO);
}

//  state.DHs = GENERATE_DH()
//  state.DHr = bob_dh_public_key
//  state.RK, state.CKs = KDF_RK(SK, DH(state.DHs, state.DHr)) 
//  state.CKr = None
//  state.Ns = 0
//  state.Nr = 0
//  state.PN = 0
//  state.MKSKIPPED = {}
static void RatchetInitAlice(CTX ctx, struct omemoState *state, const omemoKey sk, const omemoKey ekb, const struct omemoKeyPair *eka) {
  memset(state, 0, sizeof(struct omemoState));
  memcpy(&state->dhs, eka, sizeof(struct omemoKeyPair));
  memcpy(state->rk, sk, 32);
  memcpy(state->dhr, ekb, 32);
  DeriveRootKey(ctx, state, state->cks);
}

// We can remove the bundle struct all together by inlining the fields as arguments.
int omemoInitFromBundle(struct omemoSession *session, const struct omemoStore *store, const struct omemoBundle *bundle) {
  int r;
  SetupCtx(ctx);
  if (Recover(ctx, r))
    return r;
  omemoSerializedKey serspk;
  omemoSerializeKey(serspk, bundle->spk);
  if (!VerifySignature(bundle->spks, bundle->ik, serspk,
                       sizeof(omemoSerializedKey))) {
    return OMEMO_ESIG;
  }
  struct omemoKeyPair eka;
  GenerateKeyPair(ctx, &eka);
  memset(&session->state, 0, sizeof(struct omemoState));
  memcpy(session->remoteidentity, bundle->ik, 32);
  omemoKey sk;
  GetSharedSecret(ctx, sk, false, store->identity.prv, eka.prv,
                           eka.prv, bundle->ik, bundle->spk,
                           bundle->pk);
  RatchetInitAlice(ctx, &session->state, sk, bundle->spk, &eka);
  memcpy(session->pendingek, eka.pub, 32);
  session->pendingpk_id = bundle->pk_id;
  session->pendingspk_id = bundle->spk_id;
  session->fsm = SESSION_INIT;
  return 0;
}

static const struct omemoPreKey *FindPreKey(const struct omemoStore *store, uint32_t pk_id) {
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++) {
    if (store->prekeys[i].id == pk_id)
      return store->prekeys+i;
  }
  return NULL;
}

static const struct omemoSignedPreKey *FindSignedPreKey(const struct omemoStore *store, uint32_t spk_id) {
  if (spk_id == 0)
    return NULL;
  if (store->cursignedprekey.id == spk_id)
    return &store->cursignedprekey;
  if (store->prevsignedprekey.id == spk_id)
    return &store->prevsignedprekey;
  return NULL;
}

static void RotateSignedPreKey(CTX ctx, struct omemoStore *store) {
  memcpy(&store->prevsignedprekey, &store->cursignedprekey,
         sizeof(struct omemoSignedPreKey));
  GenerateSignedPreKey(ctx,
      &store->cursignedprekey,
      IncrementWrapSkipZero(store->prevsignedprekey.id),
      &store->identity);
}

//  PN = Ns
//  Ns = 0
//  Nr = 0
//  DHr = dh
//  RK, CKr = KDF_RK(RK, DH(DHs, DHr))
//  DHs = GENERATE_DH()
//  RK, CKs = KDF_RK(RK, DH(DHs, DHr))
static void DHRatchet(CTX ctx, struct omemoState *state, const omemoKey dh) {
  state->pn = state->ns;
  state->ns = 0;
  state->nr = 0;
  memcpy(state->dhr, dh, 32);
  DeriveRootKey(ctx, state, state->ckr);
  GenerateKeyPair(ctx, &state->dhs);
  DeriveRootKey(ctx, state, state->cks);
}

static void RatchetInitBob(struct omemoState *state, const omemoKey sk, const struct omemoKeyPair *ekb) {
  memcpy(&state->dhs, ekb, sizeof(struct omemoKeyPair));
  memcpy(state->rk, sk, 32);
}

static struct omemoMessageKey *FindMessageKey(struct omemoSkippedMessageKeys *keys, const omemoKey dh, uint32_t n) {
  for (int i = 0; i < keys->n; i++) {
    if (keys->p[i].nr == n && !memcmp(dh, keys->p[i].dh, 32)) {
      return keys->p + i;
    }
  }
  return NULL;
}

#define CLAMP0(v) ((v) > 0 ? (v) : 0)

static inline uint32_t GetAmountSkipped(int64_t nr, int64_t n) {
  return CLAMP0(n - nr);
}

int SkipMessageKey(CTX ctx, struct omemoKeyDecryptor *dec, size_t *nr, omemoKey dh, omemoKey mk) {
  omemoKey tmk;
  if (dec->shouldstep && dec->newstate.nr >= dec->headerpn) {
      DHRatchet(ctx, &dec->newstate, dec->headerdh);
      dec->shouldstep = false;
  }
  if ((dec->shouldstep && dec->newstate.nr < dec->headerpn)
      || dec->newstate.nr < dec->headern) {
    GetBaseMaterials(ctx, dec->newstate.ckr, tmk, dec->newstate.ckr);
    if (nr) *nr = dec->newstate.nr;
    if (dh) memcpy(dh, dec->newstate.dhr, 32);
    if (mk) memcpy(mk, tmk, 32);
    dec->newstate.nr++;
    return 0;
  }
  return 1;
}

int omemoSkipMessageKey(struct omemoKeyDecryptor *dec, size_t *nr, omemoKey dh, omemoKey mk) {
  if (!dec) return OMEMO_ESTATE;
  int r;
  SetupCtx(ctx);
  if (Recover(ctx, r)) return r;
  return SkipMessageKey(ctx, dec, nr, dh, mk);
}

// puts it in session, then you may call decrypt again, decrypt will remove it afterwards.
void omemoSupplyMessageKey(struct omemoKeyDecryptor *dec, omemoKey mk) {
  if (dec && mk) {
    dec->providedmk = true;
    memcpy(dec->mk, mk, 32);
  }
}

static void SkipMessageKeys(CTX ctx, struct omemoState *state, struct omemoSkippedMessageKeys *keys, uint32_t n) {
  assert(keys->n + GetAmountSkipped(state->nr, n) <= keys->c); // this is checked in DecryptMessage
  while (state->nr < n) {
    omemoKey mk;
    GetBaseMaterials(ctx, state->ckr, mk, state->ckr);
    keys->p[keys->n].nr = state->nr;
    memcpy(keys->p[keys->n].dh, state->dhr, 32);
    memcpy(keys->p[keys->n].mk, mk, 32);
    keys->n++;
    state->nr++;
  }
}

// Random note:
// When a skipped message key has been deleted and that message is
// attempted to be decrypted then dhr != headerdh, thus the ratchet will
// be stepped with the wrong key. However because the mac will not be
// valid, all changes to state are discarded and the dhr will be
// reverted to the last correct one.
static void DecryptMessageImpl(CTX ctx, struct omemoSession *session,
                              const struct omemoStore *store,
                              omemoKeyPayload decrypted, const uint8_t *msg,
                              size_t msgn) {
  if (msgn < 9 || msg[0] != ((3 << 4) | 3))
    Throw(ctx, OMEMO_ECORRUPT);
  struct ProtobufField fields[5] = {
    [1] = {PB_REQUIRED | PB_LEN, 33}, // ek
    [2] = {PB_REQUIRED | PB_UINT32}, // n
    [3] = {PB_REQUIRED | PB_UINT32}, // pn
    [4] = {PB_REQUIRED | PB_LEN}, // ciphertext
  };

  if (ParseProtobuf(msg+1, msgn-9, fields, 5))
    Throw(ctx, OMEMO_EPROTOBUF);
  // these checks should already be handled by ParseProtobuf, just to make sure...
  if (fields[4].v > 48 || fields[4].v < 32)
    Throw(ctx, OMEMO_ECORRUPT);
  assert(fields[1].v == 33);

  uint32_t headern = fields[2].v;
  uint32_t headerpn = fields[3].v;
  const uint8_t *headerdh = fields[1].p+1;

  bool shouldstep = !!memcmp(session->state.dhr, headerdh, 32);

  // We first check for maxskip, if that does not pass we should not
  // process the message. If it does pass, we know the total capacity of
  // the array is large enough because c >= maxskip. Then we check if the
  // new keys fit in the remaining space. If that is not the case we
  // return and let the user either remove the old message keys or ignore
  // the message.

  omemoKey mk;
  struct omemoMessageKey *key;
  // TODO: extract the entire following blocks into separate functions.
  // Also, when these functions are not called, we should still run SkipMessageKeys and the like.
  if ((key = FindMessageKey(&session->mkskipped, headerdh, headern))) {
    memcpy(mk, key->mk, 32);
    session->mkskipped.removed = key;
  } else {
    if (!shouldstep && headern < session->state.nr) Throw(ctx, OMEMO_EKEYGONE);
    uint64_t nskips = shouldstep
      ? GetAmountSkipped(session->state.nr, headerpn) + headern
      : GetAmountSkipped(session->state.nr, headern);
    if (nskips > OMEMO_MAXSKIPPED) Throw(ctx, OMEMO_EMAXSKIP);
    if (nskips > session->mkskipped.c - session->mkskipped.n) Throw(ctx, OMEMO_ESKIPBUF);
    if (shouldstep) {
      SkipMessageKeys(ctx, &session->state, &session->mkskipped, headerpn);
      DHRatchet(ctx, &session->state, headerdh);
    }
    SkipMessageKeys(ctx, &session->state, &session->mkskipped, headern);
    GetBaseMaterials(ctx, session->state.ckr, mk, session->state.ckr);
    session->state.nr++;
  }
  struct DeriveChainKeyOutput kdfout;
  DeriveChainKey(ctx, &kdfout, mk);
  uint8_t mac[8];
  GetMac(ctx, mac, session->remoteidentity, store->identity.pub, kdfout.mac, msg, msgn-8);
  if (memcmp(mac, msg+msgn-8, 8))
    Throw(ctx, OMEMO_ECORRUPT);
  uint8_t tmp[48];
  Decrypt(ctx, tmp, fields[4].p, fields[4].v, kdfout.cipher, kdfout.iv);
  memcpy(decrypted, tmp, 32);
  session->fsm = SESSION_READY;
}

static void DecryptKeyImpl(CTX ctx, struct omemoSession *session, const struct omemoStore *store, omemoKeyPayload payload, bool isprekey, const uint8_t *msg, size_t msgn) {
  if (isprekey) {
    if (msgn == 0 || msg[0] != ((3 << 4) | 3))
      Throw(ctx, OMEMO_ECORRUPT);
    // PreKeyWhisperMessage
    struct ProtobufField fields[7] = {
      [5] = {PB_REQUIRED | PB_UINT32}, // registrationid
      [1] = {PB_REQUIRED | PB_UINT32}, // prekeyid
      [6] = {PB_REQUIRED | PB_UINT32}, // signedprekeyid
      [2] = {PB_REQUIRED | PB_LEN, 33}, // basekey/ek
      [3] = {PB_REQUIRED | PB_LEN, 33}, // identitykey/ik
      [4] = {PB_REQUIRED | PB_LEN}, // message
    };
    if (ParseProtobuf(msg+1, msgn-1, fields, 7))
      Throw(ctx, OMEMO_EPROTOBUF);
    // nr will only ever be 0 with the first prekey message
    // we could put this in session->fsm...
    if (session->state.nr == 0) {
      // TODO: later remove this prekey
      const struct omemoPreKey *pk = FindPreKey(store, fields[1].v);
      const struct omemoSignedPreKey *spk = FindSignedPreKey(store, fields[6].v);
      if (!pk || !spk)
        Throw(ctx, OMEMO_ECORRUPT);
      memcpy(session->remoteidentity, fields[3].p+1, 32);
      omemoKey sk;
      GetSharedSecret(ctx, sk, true, store->identity.prv, spk->kp.prv, pk->kp.prv, fields[3].p+1, fields[2].p+1, fields[2].p+1);
      RatchetInitBob(&session->state, sk, &spk->kp);
    }
    msg = fields[4].p;
    msgn = fields[4].v;
  } else {
    if (!session->fsm) // TODO: specify which states are allowed here
      Throw(ctx, OMEMO_ESTATE);
  }
  session->fsm = SESSION_READY;
  DecryptMessageImpl(ctx, session, store, payload, msg, msgn);
}

int omemoDecryptKey(struct omemoSession *session, const struct omemoStore *store, omemoKeyPayload payload, bool isprekey, const uint8_t *msg, size_t msgn) {
  if (!session || !store || !store->isinitialized || !msg || !msgn)
    return OMEMO_ESTATE;
  //assert(session->mkskipped.p && !session->mkskipped.removed);
  struct omemoState backup;
  uint32_t mkskippednbackup = session->mkskipped.n;
  memcpy(&backup, &session->state, sizeof(struct omemoState));
  int r;
  SetupCtx(ctx);
  if (Recover(ctx, r)) {
    memcpy(&session->state, &backup, sizeof(struct omemoState));
    memset(payload, 0, OMEMO_INTERNAL_PAYLOAD_SIZE);
    session->mkskipped.n = mkskippednbackup;
    session->mkskipped.removed = NULL;
    return r;
  }
  DecryptKeyImpl(ctx, session, store, payload, isprekey, msg, msgn);
  if (session->mkskipped.removed)
    NormalizeSkipMessageKeysTrivial(&session->mkskipped);
  return 0;
}

int omemoInitKeyDecryptor(struct omemoSession *session,
                          const struct omemoStore *store, bool isprekey,
                          const uint8_t *msg, size_t msgn,
                          struct omemoKeyDecryptor *dec) {

  if (!session || !store || !store->isinitialized || !msg || !msgn)
    return OMEMO_ESTATE;
  memcpy(&dec->newstate, &session->state, sizeof(struct omemoState));
  int r;
  SetupCtx(ctx);
  if (Recover(ctx, r)) return r;
  if (isprekey) {
    if (msgn == 0 || msg[0] != ((3 << 4) | 3))
      Throw(ctx, OMEMO_ECORRUPT);
    // PreKeyWhisperMessage
    struct ProtobufField fields[7] = {
      [5] = {PB_REQUIRED | PB_UINT32}, // registrationid
      [1] = {PB_REQUIRED | PB_UINT32}, // prekeyid
      [6] = {PB_REQUIRED | PB_UINT32}, // signedprekeyid
      [2] = {PB_REQUIRED | PB_LEN, 33}, // basekey/ek
      [3] = {PB_REQUIRED | PB_LEN, 33}, // identitykey/ik
      [4] = {PB_REQUIRED | PB_LEN}, // message
    };
    if (ParseProtobuf(msg+1, msgn-1, fields, 7))
      Throw(ctx, OMEMO_EPROTOBUF);
    // nr will only ever be 0 with the first prekey message
    // we could put this in session->fsm...
    if (session->state.nr == 0) {
      // TODO: later remove this prekey
      const struct omemoPreKey *pk = FindPreKey(store, fields[1].v);
      const struct omemoSignedPreKey *spk = FindSignedPreKey(store, fields[6].v);
      if (!pk || !spk)
        Throw(ctx, OMEMO_ECORRUPT);
      memcpy(session->remoteidentity, fields[3].p+1, 32);
      omemoKey sk;
      GetSharedSecret(ctx, sk, true, store->identity.prv, spk->kp.prv, pk->kp.prv, fields[3].p+1, fields[2].p+1, fields[2].p+1);
      RatchetInitBob(&session->state, sk, &spk->kp);
    }
    msg = fields[4].p;
    msgn = fields[4].v;
  } else {
    if (!session->fsm) // TODO: specify which states are allowed here
      Throw(ctx, OMEMO_ESTATE);
  }
  session->fsm = SESSION_READY;
  if (msgn < 9 || msg[0] != ((3 << 4) | 3))
    Throw(ctx, OMEMO_ECORRUPT);
  struct ProtobufField fields[5] = {
    [1] = {PB_REQUIRED | PB_LEN, 33}, // ek
    [2] = {PB_REQUIRED | PB_UINT32}, // n
    [3] = {PB_REQUIRED | PB_UINT32}, // pn
    [4] = {PB_REQUIRED | PB_LEN}, // ciphertext
  };

  if (ParseProtobuf(msg+1, msgn-9, fields, 5))
    Throw(ctx, OMEMO_EPROTOBUF);
  // these checks should already be handled by ParseProtobuf, just to make sure...
  if (fields[4].v > 48 || fields[4].v < 32)
    Throw(ctx, OMEMO_ECORRUPT);
  assert(fields[1].v == 33);

  if (dec) {
    dec->headern = fields[2].v;
    dec->headerpn = fields[3].v;
    memcpy(dec->headerdh, fields[1].p+1, 32);
  }
  return 0;
}

int64_t omemoGetSkipAmount(const struct omemoSession *session, const struct omemoKeyDecryptor *dec) {
  return dec->shouldstep
    ? GetAmountSkipped(session->state.nr, dec->headerpn) + dec->headern
    : GetAmountSkipped(session->state.nr, dec->headern);
}

int omemoDecryptKeyNew(struct omemoSession *session, const struct omemoStore *store, struct omemoKeyDecryptor *dec, omemoKeyPayload decrypted) {
  int r;
  SetupCtx(ctx);
  if (Recover(ctx, r)) return r;
  if (!dec->providedmk) {
    while (!SkipMessageKey(ctx, dec, NULL, NULL, NULL)) {}
    GetBaseMaterials(ctx, dec->newstate.ckr, dec->mk, dec->newstate.ckr);
    session->state.nr++;
  }
  struct DeriveChainKeyOutput kdfout;
  DeriveChainKey(ctx, &kdfout, dec->mk);
  uint8_t mac[8];
  if (!VerifyMac(ctx, session->remoteidentity, store->identity.pub, dec))
    Throw(ctx, OMEMO_ECORRUPT);
  uint8_t tmp[48];
  // TODO: embed full msg inside dec?
  //Decrypt(ctx, tmp, fields[4].p, fields[4].v, kdfout.cipher, kdfout.iv);
  memcpy(decrypted, tmp, 32);
  memcpy(&session->state, &dec->newstate, sizeof(struct omemoState));
  return 0;
}

int omemoDecryptMessage(uint8_t *d, const uint8_t *payload, size_t pn, const uint8_t iv[12], const uint8_t *s, size_t n) {
  int r = 0;
  assert(pn >= 32);
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  if (!(r = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, payload, 128)))
    r = mbedtls_gcm_auth_decrypt(&ctx, n, iv, 12, "", 0, payload+16, pn-16, s, d);
  mbedtls_gcm_free(&ctx);
  return r;
}

int omemoEncryptMessage(uint8_t *d, omemoKeyPayload payload,
                        uint8_t iv[12], const uint8_t *s, size_t n) {
  int r = 0;
  if ((r = omemoRandom(payload, 16))
   || (r = omemoRandom(iv, 12)))
   return r;
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  if (!(r = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, payload, 128)))
    r = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, n, iv, 12, "", 0, s, d, 16, payload+16);
  mbedtls_gcm_free(&ctx);
  return r;
}

// TODO: can we incorporate this in ParseProtobuf?
static bool ParseRepeatingField(const uint8_t *s, size_t n,
                         struct ProtobufField *field, int fieldid) {
  int type, id;
  uint32_t v;
  const uint8_t *e = s + n;
  assert(fieldid <= 16);
  while (s < e) {
    type = *s & 7;
    id = *s >> 3;
    s++;
    if (id >= 16 || (id == fieldid && type != (field->type & 7)))
      return (assert(0), true);
    if (!(s = ParseVarInt(s, e, &v)))
      return (assert(0), true);
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
  if (s > e)
    return (assert(0), true);
  return false;
}

size_t omemoGetSerializedStoreSize(const struct omemoStore *store) {
  size_t sum = 34 * 3 + 35 * 3 + (2 + 64) * 2 + 1 * 4 +
               GetVarIntSize(store->isinitialized) +
               GetVarIntSize(store->cursignedprekey.id) +
               GetVarIntSize(store->prevsignedprekey.id) +
               GetVarIntSize(store->pkcounter);
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++)
    sum += 2 + 1 + GetVarIntSize(store->prekeys[i].id) + 34 + 35;
  return sum;
}

// TODO: use protobuf for this too
void omemoSerializeStore(uint8_t *p, const struct omemoStore *store) {
  uint8_t *d = p;
  d = FormatVarInt(d, PB_UINT32, 1, store->isinitialized);
  d = FormatPrivateKey(d, 2, store->identity.prv);
  d = FormatKey(d, 3, store->identity.pub);
  d = FormatVarInt(d, PB_UINT32, 4, store->cursignedprekey.id);
  d = FormatPrivateKey(d, 5, store->cursignedprekey.kp.prv);
  d = FormatKey(d, 6, store->cursignedprekey.kp.pub);
  d = FormatVarInt(d, PB_LEN, 7, 64);
  d = (memcpy(d, store->cursignedprekey.sig, 64), d + 64);
  // TODO: when id = 0 we don't have to include it here
  d = FormatVarInt(d, PB_UINT32, 8, store->prevsignedprekey.id);
  d = FormatPrivateKey(d, 9, store->prevsignedprekey.kp.prv);
  d = FormatKey(d, 10, store->prevsignedprekey.kp.pub);
  d = FormatVarInt(d, PB_LEN, 11, 64);
  d = (memcpy(d, store->prevsignedprekey.sig, 64), d + 64);
  d = FormatVarInt(d, PB_UINT32, 12, store->pkcounter);
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++) {
    const struct omemoPreKey *pk = store->prekeys+i;
    // TODO: only add when prekey.id != 0
    d = FormatVarInt(d, PB_LEN, 13, 1+GetVarIntSize(pk->id)+34+35);
    d = FormatVarInt(d, PB_UINT32, 1, pk->id);
    d = FormatPrivateKey(d, 2, pk->kp.prv);
    d = FormatKey(d, 3, pk->kp.pub);
  }
  assert(d-p == omemoGetSerializedStoreSize(store));
}

int omemoDeserializeStore(const char *p, size_t n, struct omemoStore *store) {
  assert(p && store);
  struct ProtobufField fields[] = {
    [1] = {PB_REQUIRED | PB_UINT32},
    [2] = {PB_REQUIRED | PB_LEN, 32},
    [3] = {PB_REQUIRED | PB_LEN, 33},
    [4] = {PB_REQUIRED | PB_UINT32},
    [5] = {PB_REQUIRED | PB_LEN, 32},
    [6] = {PB_REQUIRED | PB_LEN, 33},
    [7] = {PB_REQUIRED | PB_LEN, 64},
    [8] = {PB_REQUIRED | PB_UINT32},
    [9] = {PB_REQUIRED | PB_LEN, 32},
    [10] = {PB_REQUIRED | PB_LEN, 33},
    [11] = {PB_REQUIRED | PB_LEN, 64},
    [12] = {PB_REQUIRED | PB_UINT32},
    [13] = {/*PB_REQUIRED |*/ PB_LEN},
  };
  if (ParseProtobuf(p, n, fields, 14))
    return OMEMO_EPROTOBUF;
  store->isinitialized = fields[1].v;
  memcpy(store->identity.prv, fields[2].p, 32);
  memcpy(store->identity.pub, fields[3].p+1, 32);
  store->cursignedprekey.id = fields[4].v;
  memcpy(store->cursignedprekey.kp.prv, fields[5].p, 32);
  memcpy(store->cursignedprekey.kp.pub, fields[6].p+1, 32);
  memcpy(store->cursignedprekey.sig, fields[7].p, 64);
  store->prevsignedprekey.id = fields[8].v;
  memcpy(store->prevsignedprekey.kp.prv, fields[9].p, 32);
  memcpy(store->prevsignedprekey.kp.pub, fields[10].p+1, 32);
  memcpy(store->prevsignedprekey.sig, fields[11].p, 64);
  store->pkcounter = fields[12].v;
  const char *e = p + n;
  int i = 0;
  while (i < OMEMO_NUMPREKEYS && !ParseRepeatingField(p, e-p, &fields[13], 13) && fields[13].p) {
    struct ProtobufField innerfields[] = {
      [1] = {PB_REQUIRED | PB_UINT32},
      [2] = {PB_REQUIRED | PB_LEN, 32},
      [3] = {PB_REQUIRED | PB_LEN, 33},
    };
    if (ParseProtobuf(fields[13].p, fields[13].v, innerfields, 4))
      return OMEMO_EPROTOBUF;
    store->prekeys[i].id = innerfields[1].v;
    memcpy(store->prekeys[i].kp.prv, innerfields[2].p, 32);
    memcpy(store->prekeys[i].kp.pub, innerfields[3].p+1, 32);
    i++;
    p = fields[13].p + fields[13].v;
    fields[13].v = 0, fields[13].p = NULL;
  }
  return 0;
}

static inline uint32_t GetMessageKeySize(const struct omemoMessageKey *mk) {
  return 34 * 2 + 1 + GetVarIntSize(mk->nr); // TODO: Serialized?
}

static uint8_t *SerializeSkippedMessageKeys(uint8_t *d, const struct omemoSkippedMessageKeys *mkskipped) {
  for (int i = 0; i < mkskipped->n; i++) {
    assert(GetMessageKeySize(mkskipped->p+i) < 128);
    d = FormatVarInt(d, PB_LEN, 15, GetMessageKeySize(mkskipped->p+i));
    d = FormatVarInt(d, PB_UINT32, 1, mkskipped->p[i].nr);
    d = FormatVarInt(d, PB_LEN, 2, 32);
    d = (memcpy(d, mkskipped->p[i].dh, 32), d + 32);
    d = FormatVarInt(d, PB_LEN, 3, 32);
    d = (memcpy(d, mkskipped->p[i].mk, 32), d + 32);
  }
  return d;
}

size_t omemoGetSerializedSessionSize(
    const struct omemoSession *session) {
  uint32_t sum = 35 * 4   // SerializedKey
                 + 34 * 4 // Key
                 + 1 * 6 + GetVarIntSize(session->state.ns) +
                 GetVarIntSize(session->state.nr) +
                 GetVarIntSize(session->state.pn) +
                 GetVarIntSize(session->pendingpk_id) +
                 GetVarIntSize(session->pendingspk_id) +
                 GetVarIntSize(session->fsm);
  for (int i = 0; i < session->mkskipped.n; i++)
    sum += 2 + GetMessageKeySize(session->mkskipped.p + i);
  return sum;
}

void omemoSerializeSession(uint8_t *p, const struct omemoSession *session) {
  uint8_t *d = p;
  d = FormatKey(d, 1, session->remoteidentity);
  d = FormatPrivateKey(d, 2, session->state.dhs.prv);
  d = FormatKey(d, 3, session->state.dhs.pub);
  d = FormatKey(d, 4, session->state.dhr);
  d = FormatPrivateKey(d, 5, session->state.rk);
  d = FormatPrivateKey(d, 6, session->state.cks);
  d = FormatPrivateKey(d, 7, session->state.ckr);
  d = FormatVarInt(d, PB_UINT32, 8, session->state.ns);
  d = FormatVarInt(d, PB_UINT32, 9, session->state.nr);
  d = FormatVarInt(d, PB_UINT32, 10, session->state.pn);
  d = FormatKey(d, 11, session->pendingek);
  d = FormatVarInt(d, PB_UINT32, 12, session->pendingpk_id);
  d = FormatVarInt(d, PB_UINT32, 13, session->pendingspk_id);
  d = FormatVarInt(d, PB_UINT32, 14, session->fsm);
  d = SerializeSkippedMessageKeys(d, &session->mkskipped);
  assert(d-p == omemoGetSerializedSessionSize(session));
}

int omemoDeserializeSession(const char *p, size_t n, struct omemoSession *session) {
  assert(p && n && session);
  assert(session->mkskipped.p);
  struct ProtobufField fields[] = {
    [1] = {PB_REQUIRED | PB_LEN, 33},
    [2] = {PB_REQUIRED | PB_LEN, 32},
    [3] = {PB_REQUIRED | PB_LEN, 33},
    [4] = {PB_REQUIRED | PB_LEN, 33},
    [5] = {PB_REQUIRED | PB_LEN, 32},
    [6] = {PB_REQUIRED | PB_LEN, 32},
    [7] = {PB_REQUIRED | PB_LEN, 32},
    [8] = {PB_REQUIRED | PB_UINT32},
    [9] = {PB_REQUIRED | PB_UINT32},
    [10] = {PB_REQUIRED | PB_UINT32},
    [11] = {PB_REQUIRED | PB_LEN, 33},
    [12] = {PB_REQUIRED | PB_UINT32},
    [13] = {PB_REQUIRED | PB_UINT32},
    [14] = {PB_REQUIRED | PB_UINT32},
    [15] = {/*PB_REQUIRED |*/ PB_LEN},
  };
  if (ParseProtobuf(p, n, fields, 16))
    return OMEMO_EPROTOBUF;
  memcpy(session->remoteidentity, fields[1].p+1, 32);
  memcpy(session->state.dhs.prv, fields[2].p, 32);
  memcpy(session->state.dhs.pub, fields[3].p+1, 32);
  memcpy(session->state.dhr, fields[4].p+1, 32);
  memcpy(session->state.rk, fields[5].p, 32);
  memcpy(session->state.cks, fields[6].p, 32);
  memcpy(session->state.ckr, fields[7].p, 32);
  session->state.ns = fields[8].v;
  session->state.nr = fields[9].v;
  session->state.pn = fields[10].v;
  memcpy(session->pendingek, fields[11].p+1, 32);
  session->pendingpk_id = fields[12].v;
  session->pendingspk_id = fields[13].v;
  session->fsm = fields[14].v;
  const char *e = p + n;
  while (session->mkskipped.n < session->mkskipped.c &&
         !ParseRepeatingField(p, e - p, &fields[15], 15) &&
         fields[15].p) {
    struct ProtobufField innerfields[] = {
      [1] = {PB_REQUIRED | PB_UINT32},
      [2] = {PB_REQUIRED | PB_LEN, 32},
      [3] = {PB_REQUIRED | PB_LEN, 32},
    };
    if (ParseProtobuf(fields[15].p, fields[15].v, innerfields, 4))
      return OMEMO_EPROTOBUF;
    session->mkskipped.p[session->mkskipped.n].nr = innerfields[1].v;
    memcpy(session->mkskipped.p[session->mkskipped.n].dh, innerfields[2].p, 32);
    memcpy(session->mkskipped.p[session->mkskipped.n].mk, innerfields[3].p, 32);
    session->mkskipped.n++;
    p = fields[15].p + fields[15].v;
    fields[15].v = 0, fields[15].p = NULL;
  }
  return 0;
}
