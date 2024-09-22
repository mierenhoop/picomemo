#include <mbedtls/hkdf.h>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/random.h>

#include "c25519.h"

#include "omemo.h"

#define SESSION_UNINIT 0
#define SESSION_INIT 1
#define SESSION_READY 2

// Protobuf: https://protobuf.dev/programming-guides/encoding/

// Only supports uint32 and len prefixed (by int32).
struct ProtobufField {
  int type;
  uint32_t v;
  const uint8_t *p;
};

#define PB_REQUIRED (1 << 3)
#define PB_UINT32 0
#define PB_LEN 2

// Parse Protobuf varint. Only supports uint32, higher bits are skipped
// so it will neither overflow nor clamp to UINT32_MAX.
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
 * whether all required fields are found.
 * - Parse the value.
 * - If there already is a non-zero value specified in the field, it is
 *   used to check whether the parsed value is the same.
 * `nfields` is the amount of fields in the `fields` array. It should
 * have the value of the highest possible field number + 1. `nfields`
 * must be less than or equal to 16 because we only support a single
 * byte field number, the number is stored like this in the byte:
 * 0nnnnttt where n is the field number and t is the type.
 *
 * @param s is protobuf data
 * @param n is the length of said data
 */
static int ParseProtobuf(const uint8_t *s, size_t n,
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
      return OMEMO_EPROTOBUF;
    found |= 1 << id;
    if (!(s = ParseVarInt(s, e, &v)))
      return OMEMO_EPROTOBUF;
    if (fields[id].v && v != fields[id].v)
      return OMEMO_EPROTOBUF;
    fields[id].v = v;
    if (type == PB_LEN) {
      fields[id].p = s;
      s += fields[id].v;
    }
  }
  if (s > e)
    return OMEMO_EPROTOBUF;
  for (int i = 0; i < nfields; i++) {
    if ((fields[i].type & PB_REQUIRED) && !(found & (1 << i)))
      return OMEMO_EPROTOBUF;
  }
  return 0;
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
static size_t FormatPreKeyMessage(uint8_t d[OMEMO_PREKEYHEADER_MAXSIZE],
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
static size_t FormatMessageHeader(uint8_t d[OMEMO_HEADER_MAXSIZE], uint32_t n,
                                  uint32_t pn, const omemoKey dhs) {
  uint8_t *p = d;
  *p++ = (3 << 4) | 3;
  p = FormatKey(p, 1, dhs);
  p = FormatVarInt(p, PB_UINT32, 2, n);
  return FormatVarInt(p, PB_UINT32, 3, pn) - d;
}

// Remove the skipped message key that has just been used for
// decrypting.
//  del state.MKSKIPPED[header.dh, header.n]
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

static void c25519_sign(omemoCurveSignature sig, const omemoKey prv, const uint8_t *msg, size_t msgn) {
  assert(msgn <= 33);
  omemoKey ed;
  uint8_t msgbuf[33+64];
  int sign = 0;
  memcpy(msgbuf, msg, msgn);
  SystemRandom(msgbuf+msgn, 64);
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

static void GenerateKeyPair(struct omemoKeyPair *kp) {
  memset(kp, 0, sizeof(*kp));
  SystemRandom(kp->prv, sizeof(kp->prv));
  c25519_prepare(kp->prv);
  curve25519(kp->pub, kp->prv, c25519_base_x);
}

static void GeneratePreKey(struct omemoPreKey *pk, uint32_t id) {
  pk->id = id;
  GenerateKeyPair(&pk->kp);
}

static void GenerateIdentityKeyPair(struct omemoKeyPair *kp) {
  GenerateKeyPair(kp);
}

static void GenerateRegistrationId(uint32_t *id) {
  SystemRandom(id, sizeof(*id));
  *id = (*id % 16380) + 1;
}

static void CalculateCurveSignature(omemoCurveSignature sig, omemoKey signprv,
                                    uint8_t *msg, size_t n) {
  assert(n <= 33);
  uint8_t rnd[sizeof(omemoCurveSignature)], buf[33 + 128];
  SystemRandom(rnd, sizeof(rnd));
  c25519_sign(sig, signprv, msg, n);
}

//  DH(dh_pair, dh_pub)
static void CalculateCurveAgreement(uint8_t d[static 32], const omemoKey prv,
                                    const omemoKey pub) {

  curve25519(d, prv, pub);
}

static void GenerateSignedPreKey(struct omemoSignedPreKey *spk, uint32_t id,
                                 struct omemoKeyPair *idkp) {
  omemoSerializedKey ser;
  spk->id = id;
  GenerateKeyPair(&spk->kp);
  omemoSerializeKey(ser, spk->kp.pub);
  CalculateCurveSignature(spk->sig, idkp->prv, ser,
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

static void RefillPreKeys(struct omemoStore *store) {
  int i;
#if 1
  for (i = 0; i < 1; i++) {
    if (!store->prekeys[i].id) {
      store->pkcounter = IncrementWrapSkipZero(store->pkcounter);
      GeneratePreKey(store->prekeys+i, store->pkcounter);
    }
  }
  // HACK: for debugging keep it fast
  for (; i < OMEMO_NUMPREKEYS; i++) {
    store->pkcounter = IncrementWrapSkipZero(store->pkcounter);
    store->prekeys[i].id = store->pkcounter;
    memcpy(&store->prekeys[i].kp, &store->prekeys[0].kp, sizeof(struct omemoKeyPair));
  }
#else
  for (i = 0; i < OMEMO_NUMPREKEYS; i++) {
    if (!store->prekeys[i].id) {
      store->pkcounter = IncrementWrapSkipZero(store->pkcounter);
      GeneratePreKey(store->prekeys+i, store->pkcounter);
    }
  }
#endif
}

void omemoSetupStore(struct omemoStore *store) {
  memset(store, 0, sizeof(struct omemoStore));
  GenerateIdentityKeyPair(&store->identity);
  GenerateSignedPreKey(&store->cursignedprekey, 1, &store->identity);
  RefillPreKeys(store);
  store->isinitialized = true;
}

int omemoSetupSession(struct omemoSession *session, size_t cap) {
  memset(session, 0, sizeof(struct omemoSession));
  if (!(session->mkskipped.p = malloc(cap * sizeof(struct omemoMessageKey)))) {
    return OMEMO_EALLOC;
  }
  session->mkskipped.c = cap;
  // TODO: allow this to be set via arg or #define?
  session->mkskipped.maxskip = 1000;
  return 0;
}

void omemoFreeSession(struct omemoSession *session) {
  if (session->mkskipped.p) {
    free(session->mkskipped.p);
    session->mkskipped.p = NULL;
  }
}

//  AD = Encode(IKA) || Encode(IKB)
static void GetAd(uint8_t ad[66], const omemoKey ika, const omemoKey ikb) {
  omemoSerializeKey(ad, ika);
  omemoSerializeKey(ad + 33, ikb);
}

static int GetMac(uint8_t d[static 8], const omemoKey ika, const omemoKey ikb,
                  const omemoKey mk, const uint8_t *msg, size_t msgn) {
  assert(msgn <= OMEMO_FULLMSG_MAXSIZE);
  uint8_t macinput[66 + OMEMO_FULLMSG_MAXSIZE], mac[32];
  GetAd(macinput, ika, ikb);
  memcpy(macinput + 66, msg, msgn);
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), mk,
                      32, macinput, 66 + msgn, mac) != 0)
    return OMEMO_ECRYPTO;
  memcpy(d, mac, 8);
  return 0;
}

static int Encrypt(uint8_t out[OMEMO_PAYLOAD_MAXPADDEDSIZE], const omemoKeyPayload in, omemoKey key,
                    uint8_t iv[static 16]) {
  _Static_assert(OMEMO_PAYLOAD_MAXPADDEDSIZE == 48);
  uint8_t tmp[48];
  memcpy(tmp, in, 32);
  memset(tmp+32, 0x10, 0x10);
  mbedtls_aes_context aes;
  if (mbedtls_aes_setkey_enc(&aes, key, 256)
   || mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, 48,
                               iv, tmp, out))
    return OMEMO_ECRYPTO;
  return 0;
}

static int Decrypt(uint8_t *out, const uint8_t *in, size_t n, omemoKey key,
                    uint8_t iv[static 16]) {
  mbedtls_aes_context aes;
  if (mbedtls_aes_setkey_dec(&aes, key, 256) ||
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, n,
                               iv, in, out))
    return OMEMO_ECRYPTO;
  return 0;
}

struct __attribute__((__packed__)) DeriveChainKeyOutput {
  omemoKey cipher, mac;
  uint8_t iv[16];
};
_Static_assert(sizeof(struct DeriveChainKeyOutput) == 80);

static int DeriveChainKey(struct DeriveChainKeyOutput *out, const omemoKey ck) {
  uint8_t salt[32];
  memset(salt, 0, 32);
  return mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      salt, 32, ck, 32, "WhisperMessageKeys",
                      18, (uint8_t *)out,
                      sizeof(struct DeriveChainKeyOutput))
             ? OMEMO_ECRYPTO
             : 0;
}

// d may be the same pointer as ck
//  ck, mk = KDF_CK(ck)
static int GetBaseMaterials(omemoKey d, omemoKey mk, const omemoKey ck) {
  omemoKey tmp;
  uint8_t data = 1;
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), ck, 32, &data, 1, mk) != 0)
    return OMEMO_ECRYPTO;
  data = 2;
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), ck, 32, &data, 1, tmp) != 0)
    return OMEMO_ECRYPTO;
  memcpy(d, tmp, 32);
  return 0;
}

// CKs, mk = KDF_CK(CKs)
// header = HEADER(DHs, PN, Ns)
// Ns += 1
// return header, ENCRYPT(mk, plaintext, CONCAT(AD, header))
// msg->p                          [^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|   8   ]
static int EncryptKeyImpl(struct omemoSession *session, const struct omemoStore *store, struct omemoKeyMessage *msg, const omemoKeyPayload payload) {
  if (session->fsm != SESSION_INIT && session->fsm != SESSION_READY)
    return OMEMO_ESTATE;
  int r;
  omemoKey mk;
  struct DeriveChainKeyOutput kdfout;
  if ((r = GetBaseMaterials(session->state.cks, mk, session->state.cks)))
    return r;
  if ((r = DeriveChainKey(&kdfout, mk)))
    return r;

  msg->n = FormatMessageHeader(msg->p, session->state.ns, session->state.pn, session->state.dhs.pub);
  msg->p[msg->n++] = (4 << 3) | PB_LEN;
  msg->p[msg->n++] = OMEMO_PAYLOAD_MAXPADDEDSIZE;
  if ((r = Encrypt(msg->p+msg->n, payload, kdfout.cipher, kdfout.iv)))
    return r;
  msg->n += OMEMO_PAYLOAD_MAXPADDEDSIZE;

  if ((r = GetMac(msg->p+msg->n, store->identity.pub, session->remoteidentity, kdfout.mac, msg->p, msg->n)))
    return r;
  msg->n += 8;

  session->state.ns++;

  if (session->fsm == SESSION_INIT) {
    msg->isprekey = true;
    // [message 00...] -> [00... message] -> [header 00... message] ->
    // [header message]
    memmove(msg->p + OMEMO_PREKEYHEADER_MAXSIZE, msg->p, msg->n);
    int headersz =
        FormatPreKeyMessage(msg->p, session->pendingpk_id, session->pendingspk_id,
                            store->identity.pub, session->pendingek, msg->n);
    memmove(msg->p + headersz, msg->p + OMEMO_PREKEYHEADER_MAXSIZE, msg->n);
    msg->n += headersz;
  }
  return 0;
}

int omemoEncryptKey(struct omemoSession *session, const struct omemoStore *store, struct omemoKeyMessage *msg, const omemoKeyPayload payload) {
  int r;
  struct omemoState backup;
  memcpy(&backup, &session->state, sizeof(struct omemoState));
  memset(msg, 0, sizeof(struct omemoKeyMessage));
  if ((r = EncryptKeyImpl(session, store, msg, payload))) {
    memcpy(&session->state, &backup, sizeof(struct omemoState));
    memset(msg, 0, sizeof(struct omemoKeyMessage));
  }
  return r;
}

// RK, ck = KDF_RK(RK, DH(DHs, DHr))
static int DeriveRootKey(struct omemoState *state, omemoKey ck) {
  uint8_t secret[32], masterkey[64];
  CalculateCurveAgreement(secret, state->dhs.prv, state->dhr);
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      state->rk, 32, secret, sizeof(secret),
                      "WhisperRatchet", 14, masterkey, 64) != 0)
    return OMEMO_ECRYPTO;
  memcpy(state->rk, masterkey, 32);
  memcpy(ck, masterkey + 32, 32);
  return 0;
}


// DH1 = DH(IKA, SPKB)
// DH2 = DH(EKA, IKB)
// DH3 = DH(EKA, SPKB)
// DH4 = DH(EKA, OPKB)
// SK = KDF(DH1 || DH2 || DH3 || DH4)
static int GetSharedSecret(omemoKey sk, bool isbob, const omemoKey ika, const omemoKey ska, const omemoKey eka, const omemoKey ikb, const omemoKey spkb, const omemoKey opkb) {
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
    return OMEMO_ECRYPTO;
  uint8_t full[64];
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, 32, secret, sizeof(secret), "WhisperText", 11, full, 64) != 0)
    return OMEMO_ECRYPTO;
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
static int RatchetInitAlice(struct omemoState *state, const omemoKey sk, const omemoKey ekb, const struct omemoKeyPair *eka) {
  memset(state, 0, sizeof(struct omemoState));
  memcpy(&state->dhs, eka, sizeof(struct omemoKeyPair));
  memcpy(state->rk, sk, 32);
  memcpy(state->dhr, ekb, 32);
  if (DeriveRootKey(state, state->cks))
    return OMEMO_ECRYPTO;
  return 0;
}

// We can remove the bundle struct all together by inlining the fields as arguments.
int omemoInitFromBundle(struct omemoSession *session, const struct omemoStore *store, const struct omemoBundle *bundle) {
  int r;
  omemoSerializedKey serspk;
  omemoSerializeKey(serspk, bundle->spk);
  if (!VerifySignature(bundle->spks, bundle->ik, serspk,
                       sizeof(omemoSerializedKey))) {
    return OMEMO_ESIG;
  }
  struct omemoKeyPair eka;
  GenerateKeyPair(&eka);
  memset(&session->state, 0, sizeof(struct omemoState));
  memcpy(session->remoteidentity, bundle->ik, 32);
  omemoKey sk;
  if ((r = GetSharedSecret(sk, false, store->identity.prv, eka.prv,
                           eka.prv, bundle->ik, bundle->spk,
                           bundle->pk)))
    return r;
  if ((r = RatchetInitAlice(&session->state, sk, bundle->spk, &eka)))
    return r;
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

static void RotateSignedPreKey(struct omemoStore *store) {
  memcpy(&store->prevsignedprekey, &store->cursignedprekey,
         sizeof(struct omemoSignedPreKey));
  GenerateSignedPreKey(
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
static int DHRatchet(struct omemoState *state, const omemoKey dh) {
  int r;
  state->pn = state->ns;
  state->ns = 0;
  state->nr = 0;
  memcpy(state->dhr, dh, 32);
  if ((r = DeriveRootKey(state, state->ckr)))
    return r;
  GenerateKeyPair(&state->dhs);
  if ((r = DeriveRootKey(state, state->cks)))
    return r;
  return 0;
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

static int SkipMessageKeys(struct omemoState *state, struct omemoSkippedMessageKeys *keys, uint32_t n) {
  int r;
  assert(keys->n + (n - state->nr) <= keys->c); // this is checked in DecryptMessage
  while (state->nr < n) {
    omemoKey mk;
    if ((r = GetBaseMaterials(state->ckr, mk, state->ckr)))
      return r;
    keys->p[keys->n].nr = state->nr;
    memcpy(keys->p[keys->n].dh, state->dhr, 32);
    memcpy(keys->p[keys->n].mk, mk, 32);
    keys->n++;
    state->nr++;
  }
  assert(state->nr == n);
  return 0;
}

static int DecryptMessageImpl(struct omemoSession *session,
                              const struct omemoStore *store,
                              omemoKeyPayload decrypted, const uint8_t *msg,
                              size_t msgn) {
  int r;
  if (msgn < 9 || msg[0] != ((3 << 4) | 3))
    return OMEMO_ECORRUPT;
  struct ProtobufField fields[5] = {
    [1] = {PB_REQUIRED | PB_LEN, 33}, // ek
    [2] = {PB_REQUIRED | PB_UINT32}, // n
    [3] = {PB_REQUIRED | PB_UINT32}, // pn
    [4] = {PB_REQUIRED | PB_LEN}, // ciphertext
  };

  if ((r = ParseProtobuf(msg+1, msgn-9, fields, 5)))
    return r;
  // these checks should already be handled by ParseProtobuf, just to make sure...
  if (fields[4].v > 48 || fields[4].v < 32)
    return OMEMO_ECORRUPT;
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
  if ((key = FindMessageKey(&session->mkskipped, headerdh, headern))) {
    memcpy(mk, key->mk, 32);
    session->mkskipped.removed = key;
  } else {
    if (!shouldstep && headern < session->state.nr) return OMEMO_EKEYGONE;
    if (shouldstep && headerpn < session->state.nr) return OMEMO_EKEYGONE;
    uint64_t nskips = shouldstep ?
      headerpn - session->state.nr + headern :
      headern - session->state.nr;
    if (nskips > session->mkskipped.maxskip) return OMEMO_EMAXSKIP;
    if (nskips > session->mkskipped.c - session->mkskipped.n) return OMEMO_ESKIPBUF;
    if (shouldstep) {
      if ((r = SkipMessageKeys(&session->state, &session->mkskipped, headerpn)))
        return r;
      if ((r = DHRatchet(&session->state, headerdh)))
        return r;
    }
    if ((r = SkipMessageKeys(&session->state, &session->mkskipped, headern)))
      return r;
    if ((r = GetBaseMaterials(session->state.ckr, mk, session->state.ckr)))
      return r;
    session->state.nr++;
  }
  struct DeriveChainKeyOutput kdfout;
  if ((r = DeriveChainKey(&kdfout, mk)))
    return r;
  uint8_t mac[8];
  if ((r = GetMac(mac, session->remoteidentity, store->identity.pub, kdfout.mac, msg, msgn-8)))
    return r;
  if (memcmp(mac, msg+msgn-8, 8))
    return OMEMO_ECORRUPT;
  uint8_t tmp[48];
  if ((r = Decrypt(tmp, fields[4].p, fields[4].v, kdfout.cipher, kdfout.iv)))
    return r;
  memcpy(decrypted, tmp, 32);
  session->fsm = SESSION_READY;
  return 0;
}

static int DecryptKeyImpl(struct omemoSession *session, const struct omemoStore *store, omemoKeyPayload payload, bool isprekey, const uint8_t *msg, size_t msgn) {
  int r;
  if (isprekey) {
    if (msgn == 0 || msg[0] != ((3 << 4) | 3))
      return OMEMO_ECORRUPT;
    // PreKeyWhisperMessage
    struct ProtobufField fields[7] = {
      [5] = {PB_REQUIRED | PB_UINT32}, // registrationid
      [1] = {PB_REQUIRED | PB_UINT32}, // prekeyid
      [6] = {PB_REQUIRED | PB_UINT32}, // signedprekeyid
      [2] = {PB_REQUIRED | PB_LEN, 33}, // basekey/ek
      [3] = {PB_REQUIRED | PB_LEN, 33}, // identitykey/ik
      [4] = {PB_REQUIRED | PB_LEN}, // message
    };
    if ((r = ParseProtobuf(msg+1, msgn-1, fields, 7)))
      return r;
    // later remove this prekey
    const struct omemoPreKey *pk = FindPreKey(store, fields[1].v);
    const struct omemoSignedPreKey *spk = FindSignedPreKey(store, fields[6].v);
    if (!pk || !spk)
      return OMEMO_ECORRUPT;
    memcpy(session->remoteidentity, fields[3].p+1, 32);
    omemoKey sk;
    if ((r = GetSharedSecret(sk, true, store->identity.prv, spk->kp.prv, pk->kp.prv, fields[3].p+1, fields[2].p+1, fields[2].p+1)))
      return r;
    RatchetInitBob(&session->state, sk, &spk->kp);
    msg = fields[4].p;
    msgn = fields[4].v;
  } else {
    if (!session->fsm) // TODO: specify which states are allowed here
      return OMEMO_ESTATE;
  }
  session->fsm = SESSION_READY;
  return DecryptMessageImpl(session, store, payload, msg, msgn);
}

int omemoDecryptKey(struct omemoSession *session, const struct omemoStore *store, omemoKeyPayload payload, bool isprekey, const uint8_t *msg, size_t msgn) {
  if (!session || !store || !store->isinitialized || !msg || !msgn)
    return OMEMO_ESTATE;
  //assert(session->mkskipped.p && !session->mkskipped.removed);
  struct omemoState backup;
  uint32_t mkskippednbackup = session->mkskipped.n;
  memcpy(&backup, &session->state, sizeof(struct omemoState));
  int r;
  if ((r = DecryptKeyImpl(session, store, payload, isprekey, msg, msgn))) {
    memcpy(&session->state, &backup, sizeof(struct omemoState));
    memset(payload, 0, OMEMO_PAYLOAD_SIZE);
    session->mkskipped.n = mkskippednbackup;
    session->mkskipped.removed = NULL;
    return r;
  }
  if (session->mkskipped.removed)
    NormalizeSkipMessageKeysTrivial(&session->mkskipped);
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
                               uint8_t iv[12], const uint8_t *s,
                               size_t n) {
  int r = 0;
  SystemRandom(payload, 16);
  SystemRandom(iv, 12);
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  if (!(r = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, payload, 128)))
    r = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, n, iv, 12, "", 0, s, d, 16, payload+16);
  mbedtls_gcm_free(&ctx);
  return r;
}

size_t omemoGetSerializedStoreSize(void) {
  return sizeof(struct omemoStore);
}

// TODO: use protobuf for this too
void omemoSerializeStore(uint8_t *d, const struct omemoStore *store) {
  memcpy(d, store, sizeof(struct omemoStore));
}

void omemoDeserializeStore(struct omemoStore *store, const uint8_t s[static sizeof(struct omemoStore)]) {
  memcpy(store, s, sizeof(struct omemoStore));
}

// TODO: we might want to make this exact by checking varint sizes
size_t omemoGetSerializedSessionMaxSizeEstimate(struct omemoSession *session) {
  return 35 * 4   // SerializedKey
         + 34 * 4 // Key
         + 6 * 6  // PB_UINT32
         + 6 + session->mkskipped.n * sizeof(struct omemoMessageKey);
}

void omemoSerializeSession(uint8_t *p, size_t *n, struct omemoSession *session) {
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
  size_t bn = session->mkskipped.n*sizeof(struct omemoMessageKey);
  d = FormatVarInt(d, PB_LEN, 15, bn);
  d = (memcpy(d, session->mkskipped.p, bn), d + bn);
  if (n)
    *n = d - p;
}



/**
 * @param nmk amount of messagekeys, if it's less than there are in the
 * buffer, only the most recent ones will be deserialized
 * @return 0 or OMEMO_EPROTOBUF
 */
int omemoDeserializeSession(const char *p, size_t n, struct omemoSession *session) {
  assert(p && n && session);
  memset(session, 0, sizeof(struct omemoSession));
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
    [15] = {PB_REQUIRED | PB_LEN},
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
  session->mkskipped.c = session->mkskipped.n = fields[15].v / sizeof(struct omemoMessageKey);
  if (!(session->mkskipped.p = malloc(fields[15].v))) {
    memset(session, 0, sizeof(struct omemoSession));
    return OMEMO_EALLOC;
  }
  return 0;
}
