#include <mbedtls/hkdf.h>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

#include <sys/random.h>

#include "curve25519.h"

#define SESSION_UNINIT 0
#define SESSION_INIT 1
#define SESSION_READY 2

#define OMEMO_EPROTOBUF (-1)
#define OMEMO_ECRYPTO (-2)
#define OMEMO_ECORRUPT (-3)
#define OMEMO_ESIG (-4)
#define OMEMO_ESTATE (-5)

typedef uint8_t Key[32];
// TODO: we can just use Key for everthing
typedef Key PrivateKey;
typedef Key PublicKey;
typedef Key EdKey;

typedef uint8_t SerializedKey[1+32];
typedef uint8_t CurveSignature[64];

struct KeyPair {
  PrivateKey prv;
  PublicKey pub;
};

struct PreKey {
  uint32_t id;
  struct KeyPair kp;
};

struct SignedPreKey {
  uint32_t id;
  struct KeyPair kp;
  CurveSignature sig;
};

struct MessageKey {
  uint32_t nr;
  Key dh;
  Key ck, mk; // encryption key and mac key
  uint8_t iv[16];
};

// p is a pointer to the array of message keys with capacity c.
// the array contains n entries.
// If allowoverwrite is true, the first keys will be overwritten when there is not enough space.
// removed is NULL before calling a decryption function. When a message
// has been decrypted AND a skipped message key is used, removed will
// point to that key in array p. After this happens, it is the task of
// the API consumer to remove the key from the array and move the
// contents so that the array doesn't contain holes.
// c >= maxskip
struct SkippedMessageKeys {
  struct MessageKey _data[2000]; // TODO: remove
  struct MessageKey *p, *removed;
  size_t n, c, maxskip;
  bool allowoverwrite; // TODO: remove
};

static void NormalizeSkipMessageKeysTrivial(struct SkippedMessageKeys *s) {
  assert(s->p && s->n <= s->c);
  assert(!s->removed || s->removed < s->p + s->n);
  if (s->removed) {
    size_t n = s->n - (s->removed - s->p) - 1;
    memmove(s->removed, s->removed + 1, n * sizeof(struct SkippedMessageKeys));
    s->removed = NULL;
  }
}

struct State {
  struct KeyPair dhs;
  PublicKey dhr;
  Key rk, cks, ckr;
  uint32_t ns, nr, pn;
  //struct SkippedMessageKeys skipped;
};

#define PAYLOAD_SIZE 32
#define HEADER_MAXSIZE (2+32+2*6)
#define FULLMSG_MAXSIZE (1+HEADER_MAXSIZE+2+PAYLOAD_SIZE)
#define ENCRYPTED_MAXSIZE (FULLMSG_MAXSIZE+8)
#define PREKEYHEADER_MAXSIZE (1+18+34*2+2)

#define NUMPREKEYS 100

// [        16        |   16  ]
//  GCM encryption key GCM tag
typedef uint8_t Payload[PAYLOAD_SIZE];

// TODO: GenericMessage? we could reuse this for normal OMEMOMessages, they just don't include the PreKey header.
struct PreKeyMessage {
  uint8_t p[PREKEYHEADER_MAXSIZE+ENCRYPTED_MAXSIZE];
  size_t n;
};

// As the spec notes, a spk should be kept for one more rotation.
// If prevsignedprekey doesn't exist, its id is 0. Therefore a valid id is always >= 1;
struct Store {
  struct KeyPair identity;
  struct SignedPreKey cursignedprekey, prevsignedprekey;
  struct PreKey prekeys[NUMPREKEYS];
};

// TODO: pack for serialization?
struct Session {
  int fsm;
  PublicKey remoteidentity;
  struct State state;
};

// Random function that must not fail, if the system is not guaranteed
// to always have a random generator available, it should read from a
// pre-filled buffer.
void SystemRandom(void *d, size_t n);
// void SystemRandom(void *d, size_t n) { esp_fill_random(d, n); }

struct Bundle {
  CurveSignature spks;
  PublicKey spk, ik;
  PublicKey pk; // Randomly selected prekey
  uint32_t pk_id, spk_id;
};

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

static const uint8_t *ParseVarInt(const uint8_t *s, const uint8_t *e, uint32_t *v) {
  int i = 0;
  *v = 0;
  do {
    if (s >= e)
      return NULL;
    *v |= (*s & 0x7f) << i;
    i += 7;
    if (i > 32 - 7) // will overflow
      return NULL;
  } while (*s++ & 0x80);
  return s;
}

// ParseProtobuf parses string `s` with length `n` containing Protobuf
// data. For each field encountered it does the following:
// - Make sure the field number can be stored in `fields` and that the
//   type corresponds with the one specified in the associated field.
// - Mark the field number as found which later will be used to check whether
//   all required fields are found.
// - Parse the value.
// - If there already is a non-zero value specified in the field, it is
//   used to check whether the parsed value is the same.
// `nfields` is the amount of fields in the `fields` array. It should have the value of the highest possible
// field number + 1. `nfields` must be less than or equal to 16 because
// we only support a single byte field number, the number is stored like
// this in the byte: 0nnnnttt where n is the field number and t is the
// type.
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

static uint8_t *FormatVarInt(uint8_t d[static 6], int id, uint32_t v) {
  assert(id < 16);
  *d++ = (id << 3) | PB_UINT32;
  do {
    *d = v & 0x7f;
    v >>= 7;
    *d++ |= (!!v << 7);
  } while (v);
  return d;
}

// sizeof(d) >= 2+n
static uint8_t *FormatBytes(uint8_t *d, int id, uint8_t *b, int n) {
  assert(id < 16 && n < 128);
  *d++ = (id << 3) | PB_LEN;
  *d++ = n;
  memcpy(d, b, n);
  return d + n;
}

// PreKeyWhisperMessage without message (it should be appended right after this call)
// ek = basekey
static size_t FormatPreKeyMessage(uint8_t d[PREKEYHEADER_MAXSIZE], uint32_t pk_id, uint32_t spk_id, PublicKey ik, PublicKey ek, uint32_t msgsz) {
  assert(msgsz < 128);
  uint8_t *p = d;
  *p++ = (3 << 4) | 3; // (message->version << 4) | CIPHERTEXT_CURRENT_VERSION
  p = FormatVarInt(p, 5, 0xcc); // TODO: registration id
  p = FormatVarInt(p, 1, pk_id);
  p = FormatVarInt(p, 6, spk_id);
  p = FormatBytes(p, 3, ik, sizeof(PublicKey));
  p = FormatBytes(p, 2, ek, sizeof(PublicKey));
  *p++ = (4 << 3) | PB_LEN;
  *p++ = msgsz;
  return p - d;
}

// WhisperMessage without ciphertext
// HEADER(dh_pair, pn, n)
static size_t FormatMessageHeader(uint8_t d[HEADER_MAXSIZE], uint32_t n, uint32_t pn, PublicKey dhs) {
  uint8_t *p = d;
  *p++ = (3 << 4) | 3; // (message->version << 4) | CIPHERTEXT_CURRENT_VERSION
  p = FormatBytes(p, 1, dhs, sizeof(PublicKey));
  p = FormatVarInt(p, 2, n);
  return FormatVarInt(p, 3, pn) - d;
}

static void DumpHex(const uint8_t *p, int n, const char *msg) {
  for (int i=0;i<n;i++)
    printf("%02x", p[i]);
  printf(" << %s\n", msg);
}

static const uint8_t basepoint[32] = {9};

static void GenerateKeyPair(struct KeyPair *kp) {
  memset(kp, 0, sizeof(*kp));
  SystemRandom(kp->prv, sizeof(kp->prv));
  kp->prv[0] &= 248;
  kp->prv[31] &= 127;
  kp->prv[31] |= 64;
  curve25519_donna(kp->pub, kp->prv, basepoint);
}

static void GeneratePreKey(struct PreKey *pk, uint32_t id) {
  pk->id = id;
  GenerateKeyPair(&pk->kp);
}

static void GenerateIdentityKeyPair(struct KeyPair *kp) {
  GenerateKeyPair(kp);
}

static void GenerateRegistrationId(uint32_t *id) {
  SystemRandom(id, sizeof(*id));
  *id = (*id % 16380) + 1;
}

static void SerializeKey(SerializedKey k, Key pub) {
  k[0] = 5;
  memcpy(k + 1, pub, sizeof(SerializedKey) - 1);
}

static void CalculateCurveSignature(CurveSignature cs, Key signprv, uint8_t *msg, size_t n) {
  // TODO: OMEMO uses xed25519 and old libsignal uses curve25519
  assert(n <= 33);
  uint8_t rnd[sizeof(CurveSignature)], buf[33+128];
  SystemRandom(rnd, sizeof(rnd));
  // TODO: change this function so it doesn't fail, n will always be 33, so we will need to allocate buffer of 33+128 and pass it.
  //assert(xed25519_sign(cs, signprv, msg, n, rnd) >= 0);
  curve25519_sign(cs, signprv, msg, n, rnd, buf);
}

// AKA ECDHE
static void CalculateCurveAgreement(uint8_t d[static 32],
                                    const PublicKey pub,
                                    PrivateKey prv) {
  curve25519_donna(d, prv, pub);
}

static void GenerateSignedPreKey(struct SignedPreKey *spk, uint32_t id,
                                 struct KeyPair *idkp) {
  SerializedKey ser;
  spk->id = id;
  GenerateKeyPair(&spk->kp);
  SerializeKey(ser, spk->kp.pub);
  CalculateCurveSignature(spk->sig, idkp->prv, ser,
                          sizeof(SerializedKey));
}

static bool VerifySignature(CurveSignature sig, PublicKey sk,
                            const uint8_t *msg, size_t n) {
  return curve25519_verify(sig, sk, msg, n) == 0;
}

static void SetupStore(struct Store *store) {
  memset(store, 0, sizeof(struct Store));
  GenerateIdentityKeyPair(&store->identity);
  GenerateSignedPreKey(&store->cursignedprekey, 1, &store->identity);
  for (int i = 0; i < NUMPREKEYS; i++) {
    GeneratePreKey(store->prekeys+i, i+1);
  }
  //store->skipped.p = store->skipped._data;
  //store->skipped.c = 2000;
}

// AD = Encode(IKA) || Encode(IKB)
static void GetAd(uint8_t ad[66], Key ika, Key ikb) {
  SerializeKey(ad, ika);
  SerializeKey(ad + 33, ikb);
}

static void Encrypt(Payload out, const Payload in, Key key,
                    uint8_t iv[static 16]) {
  mbedtls_aes_context aes;
  // These functions won't fail, so we can skip error checking.
  assert(mbedtls_aes_setkey_enc(&aes, key, 256) == 0);
  assert(mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, PAYLOAD_SIZE,
                               iv, in, out) == 0);
}

static void Decrypt(Payload out, const Payload in, Key key,
                    uint8_t iv[static 16]) {
  mbedtls_aes_context aes;
  assert(mbedtls_aes_setkey_dec(&aes, key, 256) == 0);
  assert(mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, PAYLOAD_SIZE,
                               iv, in, out) == 0);
}

struct __attribute__((__packed__)) DeriveChainKeyOutput {
  Key ck, mk;
  uint8_t iv[16];
};
_Static_assert(sizeof(struct DeriveChainKeyOutput) == 80);

static int DeriveChainKey(struct DeriveChainKeyOutput *out, Key ck) {
  uint8_t salt[32];
  memset(salt, 0, 32);
  return mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      salt, 32, ck, sizeof(Key), "WhisperMessageKeys",
                      18, (uint8_t *)out,
                      sizeof(struct DeriveChainKeyOutput))
             ? OMEMO_ECRYPTO
             : 0;
}
// CKs, mk = KDF_CK(CKs)
// header = HEADER(DHs, PN, Ns)
// Ns += 1
// return header, ENCRYPT(mk, plaintext, CONCAT(AD, header))
// macinput      [   33   |   33   |   1   | <=46 |2|PAYLOAD_SIZE]
//                identity identity version header    encrypted   mac[:8]
// msg->encrypted                  [^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|   8   ]
// TODO: on fail anywhere, we must reset the state back
static int EncryptRatchet(struct Session *session, struct Store *store, struct PreKeyMessage *msg, Payload payload) {
  uint8_t macinput[66+FULLMSG_MAXSIZE], mac[32];
  struct DeriveChainKeyOutput kdfout;
  //if (session->fsm != SESSION_READY)
  //  return OMEMO_ESTATE;
  if (DeriveChainKey(&kdfout, session->state.cks))
    return OMEMO_ECRYPTO;

  GetAd(macinput, store->identity.pub, session->remoteidentity);
  int n = 66;
  n += FormatMessageHeader(macinput+n, session->state.ns, session->state.pn, session->state.dhs.pub);

  macinput[n++] = (4 << 3) | PB_LEN;
  macinput[n++] = PAYLOAD_SIZE;
  Encrypt(macinput+n, payload, kdfout.ck, kdfout.iv);
  n += PAYLOAD_SIZE;
  int encsz = n - 66;
  memcpy(msg->p, macinput+66, encsz);
  if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), kdfout.mk, 32, macinput, n, mac) != 0)
    return OMEMO_ECRYPTO;
  memcpy(msg->p+encsz, mac, 8);
  msg->n = encsz + 8;

  session->state.ns++;
  memcpy(session->state.cks, kdfout.ck, 32);
  return 0;
}

// RK, CKs = KDF_RK(SK, DH(DHs, DHr))
static int DeriveRootKey(struct State *state, Key ck) {
  uint8_t secret[32], masterkey[64];
  CalculateCurveAgreement(secret, state->dhr, state->dhs.prv);
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      state->rk, sizeof(Key), secret, sizeof(secret),
                      "WhisperRatchet", 14, masterkey, 64) != 0)
    return OMEMO_ECRYPTO;
  memcpy(state->rk, masterkey, sizeof(Key));
  memcpy(ck, masterkey + 32, 32);
  return 0;
}


// DH1 = DH(IKA, SPKB)
// DH2 = DH(EKA, IKB)
// DH3 = DH(EKA, SPKB)
// DH4 = DH(EKA, OPKB)
// SK = KDF(DH1 || DH2 || DH3 || DH4)
static int GetSharedSecret(Key sk, bool isbob, Key ika, Key ska, Key eka, const Key ikb, const Key spkb, const Key opkb) {
  uint8_t secret[32*5] = {0}, salt[32];
  memset(secret, 0xff, 32);
  // When we are bob, we must swap the first two.
  CalculateCurveAgreement(secret+32, isbob ? ikb : spkb, isbob ? ska : ika);
  CalculateCurveAgreement(secret+64, isbob ? spkb : ikb, isbob ? ika : ska);
  CalculateCurveAgreement(secret+96, spkb, ska);
  // OMEMO mandates that the bundle MUST contain a prekey.
  CalculateCurveAgreement(secret+128, opkb, eka);
  memset(salt, 0, 32);
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, 32, secret, sizeof(secret), "WhisperText", 11, sk, 32) != 0)
    return OMEMO_ECRYPTO;
  return 0;
}

// state.DHs = GENERATE_DH()
// state.DHr = bob_dh_public_key
// state.RK, state.CKs = KDF_RK(SK, DH(state.DHs, state.DHr)) 
// state.CKr = None
// state.Ns = 0
// state.Nr = 0
// state.PN = 0
// state.MKSKIPPED = {}
static int RatchetInitAlice(struct State *state, Key sk, Key ekb) {
  memset(state, 0, sizeof(struct State));
  GenerateKeyPair(&state->dhs);
  memcpy(state->rk, sk, 32);
  memcpy(state->dhr, ekb, 32);
  if (DeriveRootKey(state, state->cks))
    return OMEMO_ECRYPTO;
  return 0;
}

// When we process the bundle, we are the ones who initialize the
// session and we are referred to as alice. Otherwise we have received
// an initiation message and are called bob.
// session is initialized in this function
// msg->payload contains the payload that will be encrypted into msg->encrypted with size msg->encryptedsz (when this function returns 0)
static int EncryptFirstMessage(struct Session *session, struct Store *store, struct Bundle *bundle, struct PreKeyMessage *msg, Payload payload) {
  int r;
  SerializedKey serspk;
  memset(session, 0, sizeof(struct Session));
  SerializeKey(serspk, bundle->spk);
  if (!VerifySignature(bundle->spks, bundle->ik, serspk, sizeof(SerializedKey))) {
     return OMEMO_ESIG;
  }
  struct KeyPair eka;
  GenerateKeyPair(&eka);
  memset(&session->state, 0, sizeof(struct State));
  memcpy(session->remoteidentity, bundle->ik, sizeof(PublicKey));
  Key sk;
  if ((r = GetSharedSecret(sk, false, store->identity.prv, eka.prv, eka.prv, bundle->ik, bundle->spk, bundle->pk)))
    return r;
  RatchetInitAlice(&session->state, sk, bundle->pk);
  if ((r = EncryptRatchet(session, store, msg, payload)))
    return r;
  if (session->fsm != SESSION_READY) {
    // [message 00...] -> [00... message] -> [header 00... message] -> [header message]
    memmove(msg->p+PREKEYHEADER_MAXSIZE, msg->p, msg->n);
    int headersz = FormatPreKeyMessage(msg->p, bundle->pk_id, bundle->spk_id, store->identity.pub, eka.pub, msg->n);
    memmove(msg->p+headersz, msg->p+PREKEYHEADER_MAXSIZE, msg->n);
    msg->n += headersz;
  }
  session->fsm = SESSION_INIT;
  return 0;
}

static struct PreKey *FindPreKey(struct Store *store, uint32_t pk_id) {
  for (int i = 0; i < NUMPREKEYS; i++) {
    if (store->prekeys[i].id == pk_id)
      return store->prekeys+i;
  }
  return NULL;
}

static struct SignedPreKey *FindSignedPreKey(struct Store *store, uint32_t spk_id) {
  if (spk_id == 0)
    return NULL;
  if (store->cursignedprekey.id == spk_id)
    return &store->cursignedprekey;
  if (store->prevsignedprekey.id == spk_id)
    return &store->prevsignedprekey;
  return NULL;
}

static inline uint32_t IncrementWrapSkipZero(uint32_t n) {
  n++;
  return n + !n;
}

static void RotateSignedPreKey(struct Store *store) {
  memcpy(&store->prevsignedprekey, &store->cursignedprekey,
         sizeof(struct SignedPreKey));
  GenerateSignedPreKey(
      &store->cursignedprekey,
      IncrementWrapSkipZero(store->prevsignedprekey.id),
      &store->identity);
}

// PN = Ns
// Ns = 0
// Nr = 0
// DHr = dh
// RK, CKr = KDF_RK(RK, DH(DHs, DHr))
// DHs = GENERATE_DH()
// RK, CKs = KDF_RK(RK, DH(DHs, DHr))
static int DHRatchet(struct State *state, const Key dh) {
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

static void RatchetInitBob(struct State *state, Key sk, struct KeyPair *ekb) {
  memcpy(&state->dhs, ekb, sizeof(struct KeyPair));
  memcpy(state->rk, sk, 32);
}

// when found mk will contain the message key
static bool FindMessageKey(Key mk, struct SkippedMessageKeys *keys, Key dh, uint32_t n) {
  for (int i = 0; i < keys->n; i++) {
    if (keys->p[i].nr == n && !memcmp(dh, keys->p[i].dh, 32)) {
      memcpy(mk, keys->p[i].mk, 32);
      keys->removed = keys->p + i;
      return true;
    }
  }
  return false;
}

static struct MessageKey *PrepareMessageKeys(struct Store *store, uint32_t n) {
  return NULL;
}

#define MIN0(v) (((v) < 0) ? (v) : 0)

// Example:
// state->nr = 3
// until (header.n) = 5
// there will be two steps done.
// TODO: if the number of steps is negative, we should error because it
// was probably from a removed skipped key.
static uint64_t GetSkipSteps(const struct State *state, uint32_t until) {
  return MIN0((int64_t)until - (int64_t)state->nr);
}

static void SkipMessageKeys(struct State *state, struct SkippedMessageKeys *keys, uint32_t n) {
  assert(keys->n + n <= keys->c); // this is checked in DecryptMessage
  while (state->nr < n) {
    struct DeriveChainKeyOutput kdfout;
    assert(!DeriveChainKey(&kdfout, state->ckr));
    memcpy(state->ckr, kdfout.ck, 32);
    keys->p[keys->n].nr = state->nr;
    memcpy(keys->p[keys->n].ck, kdfout.ck, 32);
    memcpy(keys->p[keys->n].mk, kdfout.mk, 32);
    keys->n++;
    state->nr++;
  }
}

static int DecryptMessage(struct Session *session, struct Store *store, Payload decrypted, const uint8_t *msg, size_t msgn) {
  int r;
  const uint8_t *p = msg, *e = msg+msgn;
  if (session->fsm != SESSION_INIT && session->fsm != SESSION_READY)
    return OMEMO_ESTATE;
  if (msgn < 9 || *p++ != ((3 << 4) | 3))
    return OMEMO_ECORRUPT;
  e -= 8;
  const uint8_t *mac = e;
  struct ProtobufField fields[5] = {
    [1] = {PB_REQUIRED | PB_LEN, 32}, // ek
    [2] = {PB_REQUIRED | PB_UINT32}, // n
    [3] = {PB_REQUIRED | PB_UINT32}, // pn
    [4] = {PB_REQUIRED | PB_LEN, PAYLOAD_SIZE}, // ciphertext
  };

  if ((r = ParseProtobuf(p, e-p, fields, 5)))
    return r;
  // these checks should already be handled by ParseProtobuf, just to make sure...
  assert(fields[1].v == 32);
  assert(fields[4].v == PAYLOAD_SIZE);

  // TODO: we now should check whether the skipped message keys array is
  // large enough by checking header.n and header.pn with state.nr.
  // Besides that also check if header.n/header.pn doesn't exceed MAX_SKIP
  // to prevent DOS.

  uint32_t headern = fields[2].v;
  uint32_t headerpn = fields[3].v;
  const uint8_t *headerdh = fields[1].p;

  bool shouldstep = !!memcmp(session->state.dhr, headerdh, 32);
  uint64_t nskips = shouldstep ?
    GetSkipSteps(&session->state, headerpn) + headern :
    GetSkipSteps(&session->state, headern);

  // We first check for maxskip, if that does not pass we should not
  // process the message. If it does pass, we know the total capacity of
  // the array is large enough because c >= maxskip. Then we check if the
  // new keys fit in the remaining space. If that is not the case we
  // return and let the user either remove the old message keys or ignore
  // the message.

  //struct SkippedMessageKeys skipped;
  //Key mk;
  //if ((!FindMessageKey(mk, &skipped, fields[1].p, fields[2].v))) {
  //  if (!shouldstep && header.n < state.nr) return keyremoved
  //  if (shouldstep && header.pn < state.nr) return keyremoved
  //  if (nskips > skipped.max_skips) return -1;
  //  if (nskips > skipped.c-skipped.n) return -1;
  //  // do the other stuff to get mk
  //}


  if (shouldstep) {
    if ((r = DHRatchet(&session->state, fields[1].p)))
      return r;
  }

  struct DeriveChainKeyOutput kdfout;
  assert(!DeriveChainKey(&kdfout, session->state.ckr));

  Decrypt(decrypted, fields[4].p, kdfout.ck, kdfout.iv);

  session->state.nr++;
  memcpy(session->state.ckr, kdfout.ck, 32);

  session->fsm = SESSION_READY;
  return 0;
}

// Decrypt the (usually) first message and start/initialize a session.
// TODO: the prekey message can be sent multiple times, what should we do then?
static int DecryptPreKeyMessageImpl(struct Session *session, struct Store *store, Payload payload, uint8_t *p, uint8_t* e) {
  int r;
  if (e-p == 0 || *p++ != ((3 << 4) | 3))
    return OMEMO_ECORRUPT;
  // PreKeyWhisperMessage
  struct ProtobufField fields[7] = {
    [5] = {PB_REQUIRED | PB_UINT32}, // registrationid
    [1] = {PB_REQUIRED | PB_UINT32}, // prekeyid
    [6] = {PB_REQUIRED | PB_UINT32}, // signedprekeyid
    [2] = {PB_REQUIRED | PB_LEN, 32}, // basekey/ek
    [3] = {PB_REQUIRED | PB_LEN, 32}, // identitykey/ik
    [4] = {PB_REQUIRED | PB_LEN}, // message
  };
  if ((r = ParseProtobuf(p, e-p, fields, 7)))
    return r;
  assert(fields[2].v == 32);
  assert(fields[3].v == 32);
  // later remove this prekey
  struct PreKey *pk = FindPreKey(store, fields[1].v);
  if (!pk)
    return OMEMO_ECORRUPT;
  struct SignedPreKey *spk = FindSignedPreKey(store, fields[6].v);
  if (!spk)
    return OMEMO_ECORRUPT;

  memcpy(session->remoteidentity, fields[3].p, sizeof(Key));

  Key sk;
  if ((r = GetSharedSecret(sk, true, store->identity.prv, spk->kp.prv, pk->kp.prv, fields[3].p, fields[2].p, fields[2].p)))
    return r;
  RatchetInitBob(&session->state, sk, &pk->kp);

  session->fsm = SESSION_READY;
  return DecryptMessage(session, store, payload, fields[4].p, fields[4].v);
}

static int DecryptPreKeyMessage(struct Session *session, struct Store *store, Payload payload, uint8_t *msg, size_t msgn) {
  memset(session, 0, sizeof(struct Session));
  int r;
  if ((r = DecryptPreKeyMessageImpl(session, store, payload, msg, msg+msgn))) {
    memset(session, 0, sizeof(struct Session));
    memset(payload, 0, PAYLOAD_SIZE);
    return r;
  }
  return 0;
}

// pn is size of payload, some clients might make the tag larger than 16 bytes.
static void DecryptRealMessage(uint8_t *d, const uint8_t *payload, size_t pn, const uint8_t iv[12], const uint8_t *s, size_t n) {
  assert(pn >= 32);
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  assert(!mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, payload, 128));
  assert(!mbedtls_gcm_auth_decrypt(&ctx, n, iv, 12, "", 0, payload+16, pn-16, s, d));
  mbedtls_gcm_free(&ctx);
}

// payload and iv are outputs
// Both d and s have size n
static void EncryptRealMessage(uint8_t *d, Payload payload,
                               uint8_t iv[12], const uint8_t *s,
                               size_t n) {
  SystemRandom(payload, 16);
  SystemRandom(iv, 12);
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  assert(!mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, payload, 128));
  assert(!mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, n, iv, 12, "", 0, s, d, 16, payload+16));
  mbedtls_gcm_free(&ctx);
}

static void SerializeSession(uint8_t *d, struct Session *session) {
  memcpy(d, session, sizeof(struct Session));
  // TODO: message keys and crc?
}
