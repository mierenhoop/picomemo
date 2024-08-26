#include "../omemo.c"

#include "c25519.h"

// In the tests we spoof the random source as a hacky way to generate
// the exact private key we want.

void SystemRandom(void *d, size_t n) {
  assert(getrandom(d, n, 0) == n);
}

static void ClearFieldValues(struct ProtobufField *fields, int nfields) {
  for (int i = 0; i < nfields; i++) {
    fields[i].v = 0;
    fields[i].p = NULL;
  }
}

static void TestParseProtobuf() {
  struct ProtobufField fields[6] = {
    [1] = {PB_REQUIRED | PB_UINT32},
    [2] = {PB_REQUIRED | PB_UINT32},
  };
#define FatStrArgs(s) s, (sizeof(s)-1)
  assert(!ParseProtobuf(FatStrArgs("\x08\x01\x10\x80\x01"), fields, 6));
  assert(fields[1].v == 1);
  assert(fields[2].v == 0x80);
  ClearFieldValues(fields, 6);
  assert(ParseProtobuf(FatStrArgs("\x08\x01\x10\x80\x01")+1, fields, 6));
  ClearFieldValues(fields, 6);
  assert(ParseProtobuf(FatStrArgs("\x08\x01\x10\x80\x01")-1, fields, 6));
  ClearFieldValues(fields, 6);
  assert(ParseProtobuf(FatStrArgs("\x08\x01"), fields, 6));
  ClearFieldValues(fields, 6);
  assert(!ParseProtobuf(FatStrArgs("\x08\x01\x10\x80\x01\x18\x01"), fields, 6));
  assert(fields[3].v == 1);
  memset(fields, 0, sizeof(fields));
  fields[1].type = PB_REQUIRED | PB_LEN;
  fields[2].type = PB_REQUIRED | PB_UINT32;
  assert(!ParseProtobuf(FatStrArgs("\x0a\x04\xcc\xcc\xcc\xcc\x10\x01"), fields, 6));
  assert(fields[1].v == 4);
  assert(fields[1].p && !memcmp(fields[1].p, "\xcc\xcc\xcc\xcc", 4));
  assert(fields[2].v == 1);
  ClearFieldValues(fields, 6);
  assert(ParseProtobuf(FatStrArgs("\x10\x01\x0a\x04\xcc\xcc\xcc"), fields, 6));
  ClearFieldValues(fields, 6);
  fields[1].v = 3;
  assert(!ParseProtobuf(FatStrArgs("\x10\x01\x0a\x03\xcc\xcc\xcc"), fields, 6));
  ClearFieldValues(fields, 6);
  fields[1].v = 2;
  assert(ParseProtobuf(FatStrArgs("\x10\x01\x0a\x03\xcc\xcc\xcc"), fields, 6));
}

static void TestFormatProtobuf() {
  uint8_t varint[6];
  assert(FormatVarInt(varint, 1, 0x00) == varint + 2 && !memcmp(varint, "\x08\x00", 2));
  assert(FormatVarInt(varint, 1, 0x01) == varint + 2 && !memcmp(varint, "\x08\x01", 2));
  assert(FormatVarInt(varint, 1, 0x80) == varint + 3 && !memcmp(varint, "\x08\x80\x01", 3));
  assert(FormatVarInt(varint, 1, 0xffffffff) == varint + 6 && !memcmp(varint, "\x08\xff\xff\xff\xff\x0f", 6));
}

static void CopyHex(uint8_t *d, const char *hex) {
  int n = strlen(hex);
  assert(n % 2 == 0);
  n /= 2;
  for (int i = 0; i < n; i++) {
    sscanf(hex+(i*2), "%02hhx", d+i);
  }
}

static Key base = {9};

static void TestKeyPair(struct KeyPair *kp, const char *rnd, const char *prv, const char *pub) {
  Key kprv, kpub;
  CopyHex(kp->prv, rnd);
  CopyHex(kprv, prv);
  CopyHex(kpub, pub);
  c25519_prepare(kp->prv);
  assert(!memcmp(kp->prv, kprv, 32));
  curve25519_donna(kp->pub, kprv, base);
  assert(!memcmp(kpub, kp->pub, 32));
  memset(kp->pub, 0, 32);
  c25519_smult(kp->pub, c25519_base_x, kprv);
  assert(!memcmp(kpub, kp->pub, 32));
}

static void TestCurve25519() {
  struct KeyPair kpa, kpb, exp;
  uint8_t shared[32], expshared[32], rnd[32];
  TestKeyPair(&kpa, "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", "70076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c6a", "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
  TestKeyPair(&kpb, "58ab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e06b", "58ab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e06b", "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
  CopyHex(expshared, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
  CalculateCurveAgreement(shared, kpa.prv, kpb.pub);
  assert(!memcmp(expshared, shared, 32));
  CalculateCurveAgreement(shared, kpb.prv, kpa.pub);
  assert(!memcmp(expshared, shared, 32));
}

void crypto_sign_ed25519_ref10_ge_scalarmult_base(void*,const    unsigned char *);
void crypto_sign_ed25519_ref10_ge_p3_tobytes(unsigned char *,void*);

static void MontToEd(Key ed, Key prv) {
  struct {int32_t l[10][4]; } ed_pubkey_point;
  crypto_sign_ed25519_ref10_ge_scalarmult_base(&ed_pubkey_point, prv);
  crypto_sign_ed25519_ref10_ge_p3_tobytes(ed, &ed_pubkey_point);
}

static void ConvertCurvePrvToEdPub(Key ed, const Key prv) {
  struct ed25519_pt p;
  ed25519_smult(&p, &ed25519_base, prv);
  uint8_t x[F25519_SIZE];
  uint8_t y[F25519_SIZE];
  ed25519_unproject(x, y, &p);
  ed25519_pack(ed, x, y);
}

static void c25519_sign(CurveSignature sig, const Key prv, const uint8_t *msg, size_t msgn) {
  assert(msgn <= 33);
  Key ed;
  uint8_t msgbuf[33+64];
  int sign = 0;
  memcpy(msgbuf, msg, msgn);
  SystemRandom(msgbuf+msgn, 64);

  ConvertCurvePrvToEdPub(ed, prv);
  sign = ed[31] & 0x80;

  edsign_sign_modified(sig, ed, prv, msgbuf, 12);

  sig[63] &= 0x7f;
  sig[63] |= sign;
}

static bool c25519_verify(CurveSignature sig, const Key pub, const uint8_t *msg, size_t msgn) {
  Key ed;
  morph25519_mx2ey(ed, pub);
  ed[31] &= 0x7f;
  ed[31] |= sig[63] & 0x80;
  CurveSignature sig2;
  memcpy(sig2, sig, 64);
  sig2[63] &= 0x7f;
  return !!edsign_verify(sig2, ed, msg, msgn);
}

int crypto_sign_modified( unsigned char *sm, const unsigned char *m,unsigned long long mlen, const unsigned char *sk, const unsigned char* pk, const unsigned char* random);

void crypto_sign_ed25519_ref10_sc_muladd(void*,void*,void*,void*);

static void TestSign() {
  CurveSignature sig1, sig2;
  Key prv, pub;
  uint8_t msg[12];
  CopyHex(prv, "48a8892cc4e49124b7b57d94fa15becfce071830d6449004685e387"
               "c62409973");
  CopyHex(pub, "55f1bfede27b6a03e0dd389478ffb01462e5c52dbbac32cf870f00a"
               "f1ed9af3a");
  CopyHex(msg, "617364666173646661736466");
  uint8_t rnd[64];
  memset(rnd, 0xcc, 64);
  Key ed, pp;
  MontToEd(ed, prv);
  ConvertCurvePrvToEdPub(pp, prv);
  uint8_t sigbuf[128], sigbuf2[128];
  crypto_sign_modified(sigbuf, msg, 12, prv, ed, rnd);
  uint8_t msgbuf[100];
  memcpy(msgbuf, msg, 12);
  memset(msgbuf+12, 0xcc, 64);
  edsign_sign_modified(sigbuf2, ed, prv, msgbuf, 12);
  assert(!memcmp(sigbuf, sigbuf2, 64));
}

static void TestSignature() {
  Key prv, pub;
  CurveSignature sig, sig2, expsig;
  uint8_t msg[12], buf[33+128], rnd[64];
  CopyHex(prv, "48a8892cc4e49124b7b57d94fa15becfce071830d6449004685e387"
               "c62409973");
  CopyHex(pub, "55f1bfede27b6a03e0dd389478ffb01462e5c52dbbac32cf870f00a"
               "f1ed9af3a");
  CopyHex(msg, "617364666173646661736466");
  CopyHex(expsig, "2bc06c745acb8bae10fbc607ee306084d0c28e2b3bb819133392"
                  "473431291fd0dfa9c7f11479996cf520730d2901267387e08d85"
                  "bbf2af941590e3035a545285");
  assert(c25519_verify(expsig, pub, msg, 12));
  assert(curve25519_verify(expsig, pub, msg, 12) == 0);

  c25519_sign(sig, prv, msg, 12);
  assert(curve25519_verify(sig, pub, msg, 12) == 0);
  assert(c25519_verify(sig, pub, msg, 12));

  SystemRandom(rnd, 64);
  curve25519_sign(sig, prv, msg, 12, rnd, buf);
  assert(curve25519_verify(sig, pub, msg, 12) == 0);
  assert(c25519_verify(sig, pub, msg, 12));

  memset(sig, 0, 64);
  assert(!c25519_verify(sig, pub, msg, 12));
}

// This would in reality parse the bundle's XML instead of their store.
static void ParseBundle(struct Bundle *bundle, struct Store *store) {
  int pk_id = 42; // Something truly random :)
  memcpy(bundle->spks, store->cursignedprekey.sig, sizeof(CurveSignature));
  memcpy(bundle->spk, store->cursignedprekey.kp.pub, sizeof(Key));
  memcpy(bundle->ik, store->identity.pub, sizeof(Key));
  memcpy(bundle->pk, store->prekeys[pk_id-1].kp.pub, sizeof(Key));
  assert(store->prekeys[pk_id-1].id == 42);
  bundle->pk_id = store->prekeys[pk_id-1].id;
  bundle->spk_id = store->cursignedprekey.id;
}

static void TestEncryption() {
  const uint8_t *msg = "Hello there!";
  size_t n = strlen(msg);
  uint8_t encrypted[100], decrypted[100], iv[12];
  Payload payload;
  EncryptRealMessage(encrypted, payload, iv, msg, n);
  DecryptRealMessage(decrypted, payload, PAYLOAD_SIZE, iv, encrypted, n);
  assert(!memcmp(msg, decrypted, n));
}

struct TestSetup {
  struct Store storea, storeb;
  struct Session sessiona, sessionb;
};

static void MakeTestSetup(struct TestSetup *setup) {
  SetupStore(&setup->storea);
  SetupStore(&setup->storeb);
  struct Bundle bundleb;
  ParseBundle(&bundleb, &setup->storeb);
}

struct Message {
};

static void TestSession() {
  struct Store storea, storeb;
  SetupStore(&storea);
  SetupStore(&storeb);

  struct Bundle bundleb;
  ParseBundle(&bundleb, &storeb);

  struct Session sessiona, sessionb;
  Payload realpayload, payload;
  struct PreKeyMessage msg;
  memset(realpayload, 0xcc, PAYLOAD_SIZE);
  memcpy(payload, realpayload, PAYLOAD_SIZE);
  assert(EncryptFirstMessage(&sessiona, &storea, &bundleb, &msg, payload) == 0);
  memset(payload, 0, PAYLOAD_SIZE);
  assert(msg.n > 0);

  assert(DecryptPreKeyMessage(&sessionb, &storeb, payload, msg.p, msg.n) == 0);
  assert(!memcmp(realpayload, payload, PAYLOAD_SIZE));

  memset(realpayload, 0xdd, PAYLOAD_SIZE);
  memcpy(payload, realpayload, PAYLOAD_SIZE);
  assert(EncryptRatchet(&sessionb, &storeb, &msg, payload) == 0);
  assert(DecryptMessage(&sessiona, &storea, payload, msg.p, msg.n) == 0);
  assert(!memcmp(realpayload, payload, PAYLOAD_SIZE));

  memset(realpayload, 0xee, PAYLOAD_SIZE);
  memcpy(payload, realpayload, PAYLOAD_SIZE);
  assert(EncryptRatchet(&sessionb, &storeb, &msg, payload) == 0);
  assert(DecryptMessage(&sessiona, &storea, payload, msg.p, msg.n) == 0);
  assert(!memcmp(realpayload, payload, PAYLOAD_SIZE));

  memset(realpayload, 0x88, PAYLOAD_SIZE);
  memcpy(payload, realpayload, PAYLOAD_SIZE);
  assert(EncryptRatchet(&sessionb, &storeb, &msg, payload) == 0);

  Payload payload2, realpayload2;
  struct PreKeyMessage msg2;
  memset(realpayload2, 0x77, PAYLOAD_SIZE);
  memcpy(payload2, realpayload2, PAYLOAD_SIZE);
  assert(EncryptRatchet(&sessionb, &storeb, &msg2, payload2) == 0);

  assert(sessiona.mkskipped.n == 0);
  assert(DecryptMessage(&sessiona, &storea, payload2, msg2.p, msg2.n) == 0);
  assert(!memcmp(realpayload2, payload2, PAYLOAD_SIZE));
  assert(sessiona.mkskipped.n == 1);

  assert(DecryptMessage(&sessiona, &storea, payload, msg.p, msg.n) == 0);
  assert(!memcmp(realpayload, payload, PAYLOAD_SIZE));
  assert(sessiona.mkskipped.n == 0);
}

// Test session built by Gajim
static void TestReceive() {
  struct Session session;
  struct Store store;
  memset(&store, 0, sizeof(struct Store));
  store.prekeys[55].id = 56;
  CopyHex(store.prekeys[55].kp.pub, "c0a2e2216d40765490501fcf8d31892c1a4cf60ed880ae3422daa767c430916b");
  CopyHex(store.prekeys[55].kp.prv, "e8f9420a195d93f6d4acf9a5d92748aebd235bcd7648b19849882d96f8fdcf41");
  CopyHex(store.identity.pub, "4e261cb22646a9ed75a8cfa194452fd20a320634b985d45084f0d5c6fc08cc4a");
  CopyHex(store.identity.prv, "301defea859e6e440ef4a77d975c9a9590c59cddd275547adc59bf2c1d088d47");
  store.cursignedprekey.id = 1;
  CopyHex(store.cursignedprekey.kp.pub, "0a90c1ea3558b15625ad78e20861a39b1f30ca1c425a0e50557b0868821c661f");
  CopyHex(store.cursignedprekey.kp.prv, "805eb8a8982b4206d0bec56bd1e861141f2c1b48386fa35ee7231834be1dd478");
  CopyHex(store.cursignedprekey.sig, "0fa2490e4899a3da85a94093fb27e97f15d05e99bab361c9a4ca388bec6685c61d96241c0020c101854388fd41e8932a7e4fba37bc454a21a6bcc037b0407808");
  Payload payload;
  uint8_t msg[180];
  CopyHex(msg,"33083812210508a21e22879385c9f5ea5ef0a50b993167659fbc0e90614365b9d0147ac8f1201a21057f1a8715095495c17552d720975d8405c38ed11bee9404bca19062d352a9c7082252330a2105e5bbca217d32f97f860ecd3c47df86f2a71eb8d2e387e31dd1f5f5349863b455100018002220a0bae4d6e5da28a1897fa3562cd4d24ee60bc9a5d4daf0f13646239bec36a2b4fd5aa1843e12d6f128f1eaa07b3001");
  assert(DecryptPreKeyMessage(&session, &store, payload, msg, 164) == 0);
  DumpHex(payload, PAYLOAD_SIZE, "payload");

  // TODO: payload size is different here
  //CopyHex(msg, "33083812210508a21e22879385c9f5ea5ef0a50b993167659fbc0e90614365b9d0147ac8f1201a21057f1a8715095495c17552d720975d8405c38ed11bee9404bca19062d352a9c7082262330a2105e5bbca217d32f97f860ecd3c47df86f2a71eb8d2e387e31dd1f5f5349863b45510011800223081d1976144dbb3b9e7545b1c5ea72554813e51d312ae87d0e9c0ecc9c1aa01fe9cf0e7b9e371ac3f9aff79c0dc7183bbc6e0df5944062fc528f1eaa07b3001");
  //assert(DecryptPreKeyMessage(&session, &store, payload, msg, 180) == 0);
  //DumpHex(payload, PAYLOAD_SIZE, "payload");
}

static void TestDeriveChainKey() {
  static uint8_t seed[] = {
      0x8a, 0xb7, 0x2d, 0x6f, 0x4c, 0xc5, 0xac, 0x0d, 0x38, 0x7e, 0xaf,
      0x46, 0x33, 0x78, 0xdd, 0xb2, 0x8e, 0xdd, 0x07, 0x38, 0x5b, 0x1c,
      0xb0, 0x12, 0x50, 0xc7, 0x15, 0x98, 0x2e, 0x7a, 0xd4, 0x8f};


    uint8_t mk[] = {
            0xbf, 0x51, 0xe9, 0xd7, 0x5e, 0x0e, 0x31, 0x03,
            0x10, 0x51, 0xf8, 0x2a, 0x24, 0x91, 0xff, 0xc0,
            0x84, 0xfa, 0x29, 0x8b, 0x77, 0x93, 0xbd, 0x9d,
            0xb6, 0x20, 0x05, 0x6f, 0xeb, 0xf4, 0x52, 0x17};

    uint8_t mac[] = {
            0xc6, 0xc7, 0x7d, 0x6a, 0x73, 0xa3, 0x54, 0x33,
            0x7a, 0x56, 0x43, 0x5e, 0x34, 0x60, 0x7d, 0xfe,
            0x48, 0xe3, 0xac, 0xe1, 0x4e, 0x77, 0x31, 0x4d,
            0xc6, 0xab, 0xc1, 0x72, 0xe7, 0xa7, 0x03, 0x0b};

    uint8_t ck[] = {
            0x28, 0xe8, 0xf8, 0xfe, 0xe5, 0x4b, 0x80, 0x1e,
            0xef, 0x7c, 0x5c, 0xfb, 0x2f, 0x17, 0xf3, 0x2c,
            0x7b, 0x33, 0x44, 0x85, 0xbb, 0xb7, 0x0f, 0xac,
            0x6e, 0xc1, 0x03, 0x42, 0xa2, 0x46, 0xd1, 0x5d};

  Key myck, mymk;
  assert(GetBaseMaterials(myck, mymk, seed) == 0);
  assert(!memcmp(ck, myck, 32));
  struct DeriveChainKeyOutput out;
  assert(DeriveChainKey(&out, mymk) == 0);
  assert(!memcmp(mk, out.cipher, 32));
  assert(!memcmp(mac, out.mac, 32));
}

static void TestHkdf() {
  uint8_t ikm[] = {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b};

  uint8_t salt[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};

  uint8_t info[] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4,
                    0xf5, 0xf6, 0xf7, 0xf8, 0xf9};

  uint8_t okm[] = {0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90,
                   0x43, 0x4f, 0x64, 0xd0, 0x36, 0x2f, 0x2a, 0x2d, 0x2d,
                   0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c, 0x5d, 0xb0, 0x2d,
                   0x56, 0xec, 0xc4, 0xc5, 0xbf, 0x34, 0x00, 0x72, 0x08,
                   0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65};

  uint8_t out[sizeof(okm)];
  assert(mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      salt, sizeof(salt), ikm, sizeof(ikm), info,
                      sizeof(info), out,
                      sizeof(okm)) == 0);
  assert(!memcmp(okm, out, sizeof(okm)));
}

static int GetSharedSecretWithoutPreKey(Key rk, Key ck, bool isbob, Key ika, Key ska, const Key ikb, const Key spkb) {
  uint8_t secret[32*4] = {0}, salt[32];
  memset(secret, 0xff, 32);
  // When we are bob, we must swap the first two.
  CalculateCurveAgreement(secret+32, isbob ? ska : ika, isbob ? ikb : spkb);
  CalculateCurveAgreement(secret+64, isbob ? ika : ska, isbob ? spkb : ikb);
  CalculateCurveAgreement(secret+96, ska, spkb);
  memset(salt, 0, 32);
  uint8_t full[64];
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, 32, secret, sizeof(secret), "WhisperText", 11, full, 64) != 0)
    return OMEMO_ECRYPTO;
  memcpy(rk, full, 32);
  memcpy(ck, full+32, 32);
  return 0;
}

static void TestRatchet() {
  static uint8_t bobPublic[] = {
      0x05, 0x2c, 0xb4, 0x97, 0x76, 0xb8, 0x77, 0x02, 0x05, 0x74, 0x5a,
      0x3a, 0x6e, 0x24, 0xf5, 0x79, 0xcd, 0xb4, 0xba, 0x7a, 0x89, 0x04,
      0x10, 0x05, 0x92, 0x8e, 0xbb, 0xad, 0xc9, 0xc0, 0x5a, 0xd4, 0x58};

  static uint8_t bobIdentityPublic[] = {
      0x05, 0xf1, 0xf4, 0x38, 0x74, 0xf6, 0x96, 0x69, 0x56, 0xc2, 0xdd,
      0x47, 0x3f, 0x8f, 0xa1, 0x5a, 0xde, 0xb7, 0x1d, 0x1c, 0xb9, 0x91,
      0xb2, 0x34, 0x16, 0x92, 0x32, 0x4c, 0xef, 0xb1, 0xc5, 0xe6, 0x26};

  static uint8_t aliceBasePublic[] = {
      0x05, 0x47, 0x2d, 0x1f, 0xb1, 0xa9, 0x86, 0x2c, 0x3a, 0xf6, 0xbe,
      0xac, 0xa8, 0x92, 0x02, 0x77, 0xe2, 0xb2, 0x6f, 0x4a, 0x79, 0x21,
      0x3e, 0xc7, 0xc9, 0x06, 0xae, 0xb3, 0x5e, 0x03, 0xcf, 0x89, 0x50};

  static uint8_t aliceIdentityPublic[] = {
      0x05, 0xb4, 0xa8, 0x45, 0x56, 0x60, 0xad, 0xa6, 0x5b, 0x40, 0x10,
      0x07, 0xf6, 0x15, 0xe6, 0x54, 0x04, 0x17, 0x46, 0x43, 0x2e, 0x33,
      0x39, 0xc6, 0x87, 0x51, 0x49, 0xbc, 0xee, 0xfc, 0xb4, 0x2b, 0x4a};

  static uint8_t bobSignedPreKeyPublic[] = {
      0x05, 0xac, 0x24, 0x8a, 0x8f, 0x26, 0x3b, 0xe6, 0x86, 0x35, 0x76,
      0xeb, 0x03, 0x62, 0xe2, 0x8c, 0x82, 0x8f, 0x01, 0x07, 0xa3, 0x37,
      0x9d, 0x34, 0xba, 0xb1, 0x58, 0x6b, 0xf8, 0xc7, 0x70, 0xcd, 0x67};

  static uint8_t receiverAndSenderChain[] = {
      0x97, 0x97, 0xca, 0xca, 0x53, 0xc9, 0x89, 0xbb, 0xe2, 0x29, 0xa4,
      0x0c, 0xa7, 0x72, 0x70, 0x10, 0xeb, 0x26, 0x04, 0xfc, 0x14, 0x94,
      0x5d, 0x77, 0x95, 0x8a, 0x0a, 0xed, 0xa0, 0x88, 0xb4, 0x4d};
  uint8_t bobPrivate[] = {
      0xa1, 0xca, 0xb4, 0x8f, 0x7c, 0x89, 0x3f, 0xaf, 0xa9, 0x88, 0x0a,
      0x28, 0xc3, 0xb4, 0x99, 0x9d, 0x28, 0xd6, 0x32, 0x95, 0x62, 0xd2,
      0x7a, 0x4e, 0xa4, 0xe2, 0x2e, 0x9f, 0xf1, 0xbd, 0xd6, 0x5a};

  uint8_t bobIdentityPrivate[] = {
      0x48, 0x75, 0xcc, 0x69, 0xdd, 0xf8, 0xea, 0x07, 0x19, 0xec, 0x94,
      0x7d, 0x61, 0x08, 0x11, 0x35, 0x86, 0x8d, 0x5f, 0xd8, 0x01, 0xf0,
      0x2c, 0x02, 0x25, 0xe5, 0x16, 0xdf, 0x21, 0x56, 0x60, 0x5e};

  uint8_t bobSignedPreKeyPrivate[] = {
      0x58, 0x39, 0x00, 0x13, 0x1f, 0xb7, 0x27, 0x99, 0x8b, 0x78, 0x03,
      0xfe, 0x6a, 0xc2, 0x2c, 0xc5, 0x91, 0xf3, 0x42, 0xe4, 0xe4, 0x2a,
      0x8c, 0x8d, 0x5d, 0x78, 0x19, 0x42, 0x09, 0xb8, 0xd2, 0x53};


  uint8_t aliceBasePrivate[] = {
      0x11, 0xae, 0x7c, 0x64, 0xd1, 0xe6, 0x1c, 0xd5, 0x96, 0xb7, 0x6a,
      0x0d, 0xb5, 0x01, 0x26, 0x73, 0x39, 0x1c, 0xae, 0x66, 0xed, 0xbf,
      0xcf, 0x07, 0x3b, 0x4d, 0xa8, 0x05, 0x16, 0xa4, 0x74, 0x49};

  uint8_t aliceIdentityPrivate[] = {
      0x90, 0x40, 0xf0, 0xd4, 0xe0, 0x9c, 0xf3, 0x8f, 0x6d, 0xc7, 0xc1,
      0x37, 0x79, 0xc9, 0x08, 0xc0, 0x15, 0xa1, 0xda, 0x4f, 0xa7, 0x87,
      0x37, 0xa0, 0x80, 0xeb, 0x0a, 0x6f, 0x4f, 0x5f, 0x8f, 0x58};

  Key rk, ck;
  assert(GetSharedSecretWithoutPreKey(rk, ck, false, aliceIdentityPrivate, aliceBasePrivate, bobIdentityPublic+1, bobSignedPreKeyPublic+1) == 0);
  assert(!memcmp(ck, receiverAndSenderChain, 32));

  assert(GetSharedSecretWithoutPreKey(rk, ck, true, bobIdentityPrivate, bobSignedPreKeyPrivate, aliceIdentityPublic+1, aliceBasePublic+1) == 0);
  assert(!memcmp(ck, receiverAndSenderChain, 32));
}

#define RunTest(t)                                                     \
  do {                                                                 \
    puts("\e[34mRunning test " #t "\e[0m");                            \
    Test##t();                                                         \
    puts("\e[32mFinished test " #t "\e[0m");                           \
  } while (0)

int main() {
  RunTest(ParseProtobuf);
  RunTest(FormatProtobuf);
  RunTest(Curve25519);
  RunTest(Signature);
  RunTest(Encryption);
  RunTest(Session);
  RunTest(Sign);
  RunTest(Receive);
  RunTest(DeriveChainKey);
  RunTest(Hkdf);
  RunTest(Ratchet);
  puts("All tests succeeded");
}
