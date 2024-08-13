#include <mbedtls/hkdf.h>
#include <mbedtls/aes.h>

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

#include <sys/random.h>

#include "curve25519.h"

static const uint8_t basepoint[32] = {9};

typedef uint8_t Key[32];
// TODO: we can just use Key for everthing
typedef Key PrivateKey;
typedef Key PublicKey;
typedef Key EdKey;
// struct IdentityKey {PublicKey pub; EdKey ed; };
typedef uint8_t SerializedKey[1+32];
typedef uint8_t OmemoSerializedKey[32];
typedef uint8_t CurveSignature[64];

struct KeyPair {
  PrivateKey prv;
  PublicKey pub;
};

struct PreKey {
  uint32_t id;
  struct KeyPair kp;
};

struct PreKeyStore {
  struct PreKey keys[100];
};

struct SignedPreKey {
  uint32_t id;
  struct KeyPair kp;
  CurveSignature sig, omemosig;
};

struct MessageKey {
  bool exists;
  uint32_t nr;
  Key dh;
  Key mk;
};

struct State {
  struct KeyPair dhs;
  PublicKey dhr;
  Key rk, cks, ckr;
  uint32_t ns, nr, pn;
  struct MessageKey skipped[2000]; // TODO: make this a ring buffer
};

#define PAYLOAD_SIZE 32
#define HEADER_MAXSIZE (2+32+1+5+1+5)
#define FULLMSG_MAXSIZE (1+HEADER_MAXSIZE+2+PAYLOAD_SIZE)
#define ENCRYPTED_MAXSIZE (FULLMSG_MAXSIZE+8)
#define PREKEYHEADER_SIZE (1+18+34*2+2)

// TODO: pack for serialization?
struct Session {
  struct KeyPair identity;
  PublicKey remoteidentity;
  struct State state;
  bool dontsendprekeys;
};

struct EncryptedMessage {
  uint8_t payload[PAYLOAD_SIZE];
  uint8_t encrypted[PREKEYHEADER_SIZE+ENCRYPTED_MAXSIZE];
  size_t encryptedsz;
};

// Random function that must not fail, if it's really not possible to
// get random data, you should either exit the program or longjmp out.
void SystemRandom(void *d, size_t n);
// void SystemRandom(void *d, size_t n) { esp_fill_random(d, n); }

// Note: spk will be the serialized version in the XML bundle: 0x05 prepended making 33 bytes.
struct Bundle {
  CurveSignature spks;
  PublicKey spk, ik;
  PublicKey pk; // Randomly selected prekey
  uint32_t pk_id, spk_id;
  //PublicKey prekeys[150];
  //size_t prekeysn;
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


static int ParseProtobuf(const char *s, size_t n, struct ProtobufField *fields, int nfields) {
  int type, id;
  uint64_t v;
  const char *e = s + n;
  uint32_t found = 0;
  assert(nfields <= 16);
  while (s < e) {
    // This is actually a varint, but we only support id < 16 and return an
    // error otherwise, so we don't have to account for multiple-byte tags.
    type = *s & 7;
    id = *s >> 3;
    s++;
    if (id >= nfields || type != (fields[id].type & 7))
      return -1;
    found |= 1 << id;
    if (!(s = ParseVarInt(s, e, &fields[id].v)))
      return -1;
    if (type == PB_LEN) {
      fields[id].p = s;
      s += fields[id].v;
    }
  }
  for (int i = 0; i < nfields; i++) {
    if ((fields[i].type & PB_REQUIRED) && !(found & (1 << i)))
      return -1;
  }
  return 0;
}

static int ParseKeyExchange(const uint8_t *s, size_t n) {
  int r;
  struct ProtobufField fields[6] = {
    [1] = {PB_REQUIRED | PB_UINT32},
    [2] = {PB_REQUIRED | PB_UINT32},
    [3] = {PB_REQUIRED | PB_LEN},
    [4] = {PB_REQUIRED | PB_LEN},
    [5] = {PB_REQUIRED | PB_LEN},
  };
  if ((r = ParseProtobuf(s, n, fields, 6)))
    return r;
  return 0;
}

static uint8_t *FormatVarInt(uint8_t d[static 5], uint32_t v) {
  do {
    *d = v & 0x7f;
    v >>= 7;
    *d++ |= (!!v << 7);
  } while (v);
  return d;
}

// sizeof(d) == 2+n
static uint8_t *FormatBytes(uint8_t *d, int id, uint8_t *b, int n) {
  assert(id < 16 && n < 128);
  *d++ = (id << 3) | PB_LEN;
  *d++ = n;
  memcpy(d, b, n);
  return d + n;
}

// PreKeyWhisperMessage without message (it should be appended right after this call)
// ek = basekey
static uint8_t *FormatPreKeyMessage(uint8_t d[PREKEYHEADER_SIZE], uint32_t pk_id, uint32_t spk_id, PublicKey ik, PublicKey ek, uint32_t msgsz) {
  assert(msgsz < 128);
  *d++ = (3 << 4) | 3; // (message->version << 4) | CIPHERTEXT_CURRENT_VERSION
  *d++ = (5 << 3) | PB_UINT32;
  d = FormatVarInt(d, 0xcc); // TODO: registration id
  *d++ = (1 << 3) | PB_UINT32;
  d = FormatVarInt(d, pk_id);
  *d++ = (6 << 3) | PB_UINT32;
  d = FormatVarInt(d, spk_id);
  d = FormatBytes(d, 3, ik, sizeof(PublicKey));
  d = FormatBytes(d, 2, ek, sizeof(PublicKey));
  *d++ = (4 << 3) | PB_LEN;
  *d++ = msgsz;
  return d;
}

// WhisperMessage without ciphertext
// HEADER(dh_pair, pn, n)
static uint8_t *FormatMessageHeader(uint8_t d[HEADER_MAXSIZE], uint32_t n, uint32_t pn, PublicKey dhs) {
  *d++ = (3 << 4) | 3; // (message->version << 4) | CIPHERTEXT_CURRENT_VERSION
  FormatBytes(d, 1, dhs, sizeof(PublicKey));
  *d++ = (2 << 3) | PB_UINT32;
  d = FormatVarInt(d, n);
  *d++ = (3 << 3) | PB_UINT32;
  return FormatVarInt(d, pn);
}

static void GenerateKeyPair(struct KeyPair *kp) {
  memset(kp, 0, sizeof(*kp));
  SystemRandom(kp->prv, sizeof(kp->prv));
  kp->prv[0] &= 248;
  kp->prv[31] &= 127;
  kp->prv[31] |= 64;
  // It always returns 0, no err checking needed.
  curve25519_donna(kp->pub, kp->prv, basepoint);
}

static void GeneratePreKey(struct PreKey *pk, uint32_t id) {
    pk->id = id;
    GenerateKeyPair(&pk->kp);
}

static void GeneratePreKeys(struct PreKeyStore *store) {
  for (int i = 0; i < 100; i++) {
    GeneratePreKey(&store->keys[i], i);
  }
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
  memcpy(k+1, pub, sizeof(SerializedKey)-1);
}

static void SerializeKeyOmemo(OmemoSerializedKey sk, Key pub) {
  memcpy(sk, pub, sizeof(OmemoSerializedKey));
}

static int CalculateCurveSignature(CurveSignature cs, Key signprv, const uint8_t *msg, size_t n) {
  // TODO: OMEMO uses xed25519 and old libsignal uses curve25519
  uint8_t rnd[sizeof(CurveSignature)];
  SystemRandom(rnd, sizeof(rnd));
  // TODO: change this function so it doesn't fail
  assert(xed25519_sign(cs, signprv, msg, n, rnd) >= 0);
  //assert(curve25519_sign(cs, signprv, msg, n, rnd) >= 0);
  return 0;
}

static void DecodeEdPoint(PublicKey pub, EdKey ed) {
  fe y, u;
  crypto_sign_ed25519_ref10_fe_frombytes(y, ed);
  fe_edy_to_montx(u, y);
  crypto_sign_ed25519_ref10_fe_tobytes(pub, u);
}

static void DecodePointMont(PublicKey pub, PublicKey data) {
  memcpy(pub, data, sizeof(PublicKey));
}

static void DecodePrivatePoint(PrivateKey prv, PrivateKey data) {
  memcpy(prv, data, sizeof(PrivateKey));
}

// AKA ECDHE
static void CalculateCurveAgreement(uint8_t d[static 32], PublicKey pub, PrivateKey prv) {
  curve25519_donna(d, prv, pub);
}

static void GenerateSignedPreKey(struct SignedPreKey *spk, uint32_t id, struct KeyPair *idkp) {
  SerializedKey sk;
  OmemoSerializedKey omemok;
  spk->id = id;
  GenerateKeyPair(&spk->kp);
  SerializeKey(sk, spk->kp.pub);
  SerializeKeyOmemo(omemok, spk->kp.pub);
  CalculateCurveSignature(spk->sig, idkp->prv, sk, sizeof(SerializedKey));
  CalculateCurveSignature(spk->omemosig, idkp->prv, omemok, sizeof(OmemoSerializedKey));
}

static bool VerifySignature(CurveSignature sig, PublicKey sk, const uint8_t *msg, size_t n) {
  return curve25519_verify(sig, sk, msg, n) == 0;
}

// What this function would do in libomemo-c is get the identity keypair
// from a callback and memcpy everything.
//static void GetIdentityKeyPair(struct KeyPair *ouridkeypair) {
//  PublicKey pub;
//  PrivateKey prv;
//  // TODO: check if it's always plain pub key and not ed or Serialized
//  DecodePointMont(pub, ouridkeypair->pub);
//  DecodePrivatePoint(prv, ouridkeypair->prv);
//}

// CKs, mk = KDF_CK(CKs)
// header = HEADER(DHs, PN, Ns)
// Ns += 1
// return header, ENCRYPT(mk, plaintext, CONCAT(AD, header))
// macinput      [   33   |   33   |   1   | <=46 |2|PAYLOAD_SIZE]
//                identity identity version header    encrypted   mac[:8]
// msg->encrypted                  [^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|   8   ]
// TODO: keep sending prekeymessages until we receive something
static void EncryptRatchet(struct Session *session, struct EncryptedMessage *msg) {
  uint8_t salt[32], output[80], macinput[33*2+FULLMSG_MAXSIZE], mac[32], encrypted[PAYLOAD_SIZE];
  mbedtls_aes_context aes;
  memset(salt, 0, 32);
  assert(mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, 32, session->state.cks, sizeof(Key), "WhisperMessageKeys", 18, output, 80) == 0);

  SerializeKey(macinput, session->identity.pub);
  SerializeKey(macinput+33, session->remoteidentity);
  int n = FormatMessageHeader(macinput+33*2, session->state.ns, session->state.pn, session->state.dhs.pub) - macinput;

  assert(mbedtls_aes_setkey_enc(&aes, output, 128) == 0);
  assert(mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, PAYLOAD_SIZE, output+64, msg->payload, encrypted) == 0);
  // TODO: we should inline this function, size is constant anyways
  n = FormatBytes(macinput+n, 4, encrypted, PAYLOAD_SIZE) - macinput;

  int encsz = n - 33*2;
  memcpy(msg->encrypted, macinput+33*2, encsz);
  assert(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), output+32, 32, macinput, n, mac) == 0);
  memcpy(msg->encrypted+encsz, mac, 8);
  msg->encryptedsz = encsz + 8;

  // nothing can fail anymore so we save the new state
  session->state.ns++;
  memcpy(session->state.cks, output, 32);
}

// RK, CKs = KDF_RK(SK, DH(DHs, DHr))
static void CalculateSendingRatchet(struct Session *session) {
  uint8_t secret[32], masterkey[64];
  CalculateCurveAgreement(secret, session->state.dhr, session->state.dhs.prv);
  assert(mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), session->state.rk, sizeof(Key), secret, sizeof(secret), "WhisperRatchet", 14, masterkey, 64) == 0);
  memcpy(session->state.rk, masterkey, sizeof(Key));
  memcpy(session->state.cks, masterkey+32, 32);
}

// DH1 = DH(IKA, SPKB)
// DH2 = DH(EKA, IKB)
// DH3 = DH(EKA, SPKB)
// DH4 = DH(EKA, OPKB)
// SK = KDF(DH1 || DH2 || DH3 || DH4)
//
// DHs = GENERATE_DH()
static void InitSessionAlice(struct Bundle *bundle, struct Session *session, struct KeyPair *base) {
  uint8_t secret[32*5];
  memset(secret, 255, 32);
  CalculateCurveAgreement(secret+32, bundle->spk, session->identity.prv);
  CalculateCurveAgreement(secret+64, bundle->ik, base->prv);
  CalculateCurveAgreement(secret+96, bundle->spk, base->prv);
  // OMEMO mandates that the bundle MUST contain a prekey.
  CalculateCurveAgreement(secret+128, bundle->pk, base->prv);

  uint8_t masterkey[64];
  uint8_t salt[32];
  memset(salt, 0, 32);
  assert(mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, 32, secret, sizeof(secret), "WhisperText", 11, masterkey, 64) == 0);

  memcpy(session->state.rk, masterkey, sizeof(Key));
  memcpy(session->state.dhr, bundle->spk, sizeof(Key));
  GenerateKeyPair(&session->state.dhs);
  CalculateSendingRatchet(session);
}

// When we process the bundle, we are the ones who initialize the
// session and we are referred to as alice. Otherwise we have received
// an initiation message and are called bob.
// session is initialized in this function
// msg->payload contains the payload that will be encrypted into msg->encrypted with size msg->encryptedsz (when this function returns 0)
static int ProcessBundle(struct Session *s, struct Bundle *b, struct EncryptedMessage *msg) {
  SerializedKey serspk;
  struct KeyPair ourbasekey;
  memset(s, 0, sizeof(struct Session));
  SerializeKey(serspk, b->spk);
  if (!VerifySignature(b->spks, b->ik, serspk, sizeof(SerializedKey))) {
     return -1;
  }
  GenerateKeyPair(&ourbasekey);
  memset(&s->state, 0, sizeof(struct State));
  memcpy(s->remoteidentity, b->ik, sizeof(PublicKey));
  InitSessionAlice(b, s, &ourbasekey);
  EncryptRatchet(s, msg);
  if (!s->dontsendprekeys) {
    memmove(msg->encrypted+PREKEYHEADER_SIZE, msg->encrypted, msg->encryptedsz);
    FormatPreKeyMessage(msg->encrypted, b->pk_id, b->spk_id, s->identity.pub, ourbasekey.pub, msg->encryptedsz);
    msg->encryptedsz += PREKEYHEADER_SIZE;
  }

  return 0;
}

// msg->encrypted contains the payload with size msg->encryptedsz that will be decrypted into msg->payload (when this function returns 0)
static void ProcessPreKeyMessage(struct Session *session, struct EncryptedMessage *msg) {
  char *p = msg->encrypted;
  char *e = p+msg->encryptedsz;
  assert(e-p >= PREKEYHEADER_SIZE);
  assert(*p++ == ((3 << 4) | 3));
}
