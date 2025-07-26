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

#include "../omemo.c"
#include "../hacl.h"
#include "../c25519.h"

#include <stdlib.h>
#include <stdio.h>

#include <sys/random.h>

#include "o/store.inc"

// TODO: for all exposed functions, test all error paths

int omemoRandom(void *d, size_t n) { return getrandom(d, n, 0) != n; }

static void CopyHex(uint8_t *d, const char *hex) {
  int n = strlen(hex);
  assert(n % 2 == 0);
  n /= 2;
  for (int i = 0; i < n; i++) {
    sscanf(hex+(i*2), "%02hhx", d+i);
  }
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

static void TestProtobufPrekey() {
  uint8_t msg[184];
  size_t msgn = 184;
  CopyHex(msg, "08041221054fb4dacf2d54cea8bd3be51dc90e1f5af444886facaf84b83d0f3031eff961791a210516260835a3c627dbbc17e3a0c32d6fbee27bed265977ae3eff1cc56f31deea212262330a21054751b36ba17d6a3a158c87660063c1cdede4a99be91e301066b3d9adb82e9f08100418002230060bd96226d3b5ef5bc642316e41e3a7f16ecfcc2f718d66655ab84e08fba1818e238e2917e025c997329395f885e98bfe98138e7f64edf528adc2debf0430cd8baf9c05");
  struct ProtobufField fields[7] = {
      [5] = {PB_REQUIRED | PB_UINT32},  // registrationid
      [1] = {PB_REQUIRED | PB_UINT32},  // prekeyid
      [6] = {PB_REQUIRED | PB_UINT32},  // signedprekeyid
      [2] = {PB_REQUIRED | PB_LEN, 33}, // basekey/ek
      [3] = {PB_REQUIRED | PB_LEN, 33}, // identitykey/ik
      [4] = {PB_REQUIRED | PB_LEN},     // message
  };
  assert(!ParseProtobuf(msg, msgn, fields, 7));
}

static void TestFormatProtobuf() {
  uint8_t varint[6];
  assert(FormatVarInt(varint, PB_UINT32, 1, 0x00) == varint + 2 && !memcmp(varint, "\x08\x00", 2));
  assert(FormatVarInt(varint, PB_UINT32, 1, 0x01) == varint + 2 && !memcmp(varint, "\x08\x01", 2));
  assert(FormatVarInt(varint, PB_UINT32, 1, 0x80) == varint + 3 && !memcmp(varint, "\x08\x80\x01", 3));
  assert(FormatVarInt(varint, PB_UINT32, 1, 0xffffffff) == varint + 6 && !memcmp(varint, "\x08\xff\xff\xff\xff\x0f", 6));
  assert(GetVarIntSize(0) == 1);
  assert(GetVarIntSize(10) == 1);
  assert(GetVarIntSize(0x80) == 2);
  assert(GetVarIntSize(UINT32_MAX) == 5);
}

static void TestFormattings() {
  uint8_t buf[OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE+OMEMO_INTERNAL_ENCRYPTED_MAXSIZE];
  omemoKey k = {0};
  assert(FormatPreKeyMessage(buf, UINT32_MAX, UINT32_MAX, k, k, OMEMO_INTERNAL_ENCRYPTED_MAXSIZE) == OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE);
  assert(FormatMessageHeader(buf, UINT32_MAX, UINT32_MAX, k) == OMEMO_INTERNAL_HEADER_MAXSIZE);
}

static void TestEncryptSize() {
  struct omemoStore store;
  assert(!omemoDeserializeStore(store_inc, store_inc_len, &store));
  // TODO: fill all keys with random values??
  struct omemoSession session = {0};
  session.init = SESSION_INIT;
  session.state.ns = UINT32_MAX;
  session.state.pn = UINT32_MAX;
  session.usedpk_id = UINT32_MAX;
  session.usedspk_id = UINT32_MAX;
  struct omemoKeyMessage msg;
  omemoKeyPayload payload;
  assert(!EncryptKeyImpl(&session, &store, &msg, payload));
  assert(msg.n == sizeof(msg.p));
}

static void TestKeyPair(struct omemoKeyPair *kp, const char *rnd, const char *prv, const char *pub) {
  omemoKey kprv, kpub;
  CopyHex(kp->prv, rnd);
  CopyHex(kprv, prv);
  CopyHex(kpub, pub);
  c25519_prepare(kp->prv);
  assert(!memcmp(kp->prv, kprv, 32));
#ifndef __SIZEOF_INT128__
#warning "Not testing the faster curve25519 implementation because system doesn't support 128bit integers."
#endif
  c25519_smult(kp->pub, c25519_base_x, kprv);
  assert(!memcmp(kpub, kp->pub, 32));
  memset(kp->pub, 0, 32);
  Hacl_Curve25519_51_secret_to_public(kp->pub, kprv);
  assert(!memcmp(kpub, kp->pub, 32));
}

static void TestCurve25519() {
  struct omemoKeyPair kpa, kpb;
  uint8_t shared[32], expshared[32];
  TestKeyPair(&kpa, "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", "70076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c6a", "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
  TestKeyPair(&kpb, "58ab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e06b", "58ab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e06b", "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
  CopyHex(expshared, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
  c25519_smult(shared, kpb.pub, kpa.prv);
  assert(!memcmp(expshared, shared, 32));
  c25519_smult(shared, kpa.pub, kpb.prv);
  assert(!memcmp(expshared, shared, 32));
  assert(Hacl_Curve25519_51_ecdh(shared, kpa.prv, kpb.pub));
  assert(!memcmp(expshared, shared, 32));
  assert(Hacl_Curve25519_51_ecdh(shared, kpb.prv, kpa.pub));
  assert(!memcmp(expshared, shared, 32));
}

static void TestRotate() {
  struct omemoStore store;
  assert(!omemoDeserializeStore(store_inc, store_inc_len, &store));
  struct omemoSignedPreKey spk;
  memcpy(&spk, &store.cursignedprekey, sizeof(spk));
  assert(!omemoRotateSignedPreKey(&store));
  assert(!memcmp(&spk, &store.prevsignedprekey, sizeof(spk)));
  assert(memcmp(&spk, &store.cursignedprekey, sizeof(spk)));
}

static void TestSignature() {
  struct omemoKeyPair ik;
  omemoCurveSignature sig, expsig;
#ifndef OMEMO2
#define MSGLEN 12
  uint8_t msg[MSGLEN];
  CopyHex(msg, "617364666173646661736466");
  CopyHex(ik.prv, "48a8892cc4e49124b7b57d94fa15becfce071830d6449004685e387"
               "c62409973");
  CopyHex(ik.pub, "55f1bfede27b6a03e0dd389478ffb01462e5c52dbbac32cf870f00a"
               "f1ed9af3a");
  CopyHex(expsig, "2bc06c745acb8bae10fbc607ee306084d0c28e2b3bb819133392"
                  "473431291fd0dfa9c7f11479996cf520730d2901267387e08d85"
                  "bbf2af941590e3035a545285");
#else
#define MSGLEN 2
  uint8_t msg[MSGLEN];
  CopyHex(ik.prv, "909a8b755ed902849023a55b15c23d11ba4d7f4ec5c2f51b1325a181991ea95c");
  CopyHex(ik.pub, "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025");
  CopyHex(msg, "af82");
  CopyHex(expsig, "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a");
#endif
  assert(VerifySignature(expsig, ik.pub, msg, MSGLEN));

  uint8_t rnd[64];
  assert(!omemoRandom(rnd, 64));
  assert(!CalculateCurveSignature(sig, &ik, rnd, msg, MSGLEN));
  assert(VerifySignature(sig, ik.pub, msg, MSGLEN));

#ifndef OMEMO2
  uint8_t msg2[33];
  memset(ik.prv, 0x22, 32);
  c25519_prepare(ik.prv);
  memset(msg2, 0xaa, 33);
  memset(rnd, 0x55, 64);
  CalculateCurveSignature(sig, &ik, rnd, msg2, 33);
  CopyHex(expsig, "f233b4ff4a5ba228980348fc07a49bdb26d4c88499015b29c604995cbe8c98351934e773569453d17ee000011e3662783d695f830b6a4bb49fb774c9b0599604");
  assert(!memcmp(expsig, sig, 64));
#endif
}

void TestCurvePrvToEdPub() {
  omemoKey prv, pub2, pub3;
  memset(prv, 0xcc, 32);
  c25519_prepare(prv);
  edsign_sm_pack(pub2, prv);
  Hacl_Ed25519_pub_from_Curve25519_priv(pub3, prv);
  assert(!memcmp(pub2, pub3, 32));
}

void TestSignModified() {
  omemoCurveSignature sig, sig2;
  uint8_t msgbuf[32+64];
  omemoKey seed, prv, pub;
  memset(seed, 0xcc, 32);
  memset(msgbuf, 0x55, 32);
  memset(msgbuf+32, 0xaa, 64);
  Hacl_Ed25519_seed_to_pub_priv(pub, prv, seed);
  edsign_sign_modified(sig, pub, prv, msgbuf, 32);
  Hacl_Ed25519_sign_modified(sig2, pub, prv, msgbuf, 32);
  assert(!memcmp(sig, sig2, 64));
}

void TestEdPubToCurvePub() {
  omemoKey seed, prv1, prv2, pub1, pub2, pub3, pub4;
  memset(seed, 0xee, 32);
  edsign_sec_to_pub(pub1, prv1, seed);
  Hacl_Ed25519_seed_to_pub_priv(pub2, prv2, seed);
  assert(!memcmp(prv1, prv2, 32));
  assert(!memcmp(pub1, pub2, 32));
  pub1[31] &= 0x7f;
  pub2[31] &= 0x7f;
  morph25519_e2m(pub3, pub1);
  Hacl_Ed25519_pub_to_Curve25519_pub(pub4, pub2);
  assert(!memcmp(pub3, pub4, 32));
}

void TestCurvePubToEdPub() {
  struct omemoKeyPair kp;
  omemoKey ed1, ed2;
  assert(!GenerateKeyPair(&kp));
  Hacl_Curve25519_pub_to_Ed25519_pub(ed1, kp.pub);
  morph25519_mx2ey(ed2, kp.pub);
  assert(!memcmp(ed1, ed2, 32));
}

void TestKeyConversions() {
  TestCurvePrvToEdPub();
  TestEdPubToCurvePub();
  TestCurvePubToEdPub();
}

// This would in reality parse the bundle's XML instead of their store.
static void ParseBundle(struct omemoBundle *bundle, struct omemoStore *store) {
  int pk_id = 42; // Something truly random :)
  memcpy(bundle->spks, store->cursignedprekey.sig, sizeof(omemoCurveSignature));
  memcpy(bundle->spk, store->cursignedprekey.kp.pub, sizeof(omemoKey));
  memcpy(bundle->ik, store->identity.pub, sizeof(omemoKey));
  memcpy(bundle->pk, store->prekeys[pk_id-1].kp.pub, sizeof(omemoKey));
  assert(store->prekeys[pk_id-1].id == 42);
  bundle->pk_id = store->prekeys[pk_id-1].id;
  bundle->spk_id = store->cursignedprekey.id;
}

static void TestEncryption() {
  const uint8_t *msg = "Hello there!";
  size_t n = strlen(msg);
  uint8_t encrypted[100], decrypted[100];
  strcpy(decrypted, msg);
  omemoKeyPayload payload;
#ifdef OMEMO2
  assert(!omemoEncryptMessage(encrypted, payload, decrypted, n));
  memset(decrypted, 0, sizeof(decrypted));
  assert(!omemoDecryptMessage(decrypted, &n, payload, sizeof(omemoKeyPayload), encrypted, n+omemoGetMessagePadSize(n)));
#else
  uint8_t iv[12];
  assert(!omemoEncryptMessage(encrypted, payload, iv, decrypted, n));
  memset(decrypted, 0, sizeof(decrypted));
  assert(!omemoDecryptMessage(decrypted, payload, sizeof(omemoKeyPayload), iv, encrypted, n));
#endif
  assert(!memcmp(msg, decrypted, n));

#ifdef OMEMO2
  CopyHex(payload, "6820575d498eff7babf1d1c3e23358a515e439ac9b649c5a6b631256a7851f3f962a8aeecb68c146333aa690ff516f2f");
  CopyHex(encrypted, "84890dc74a7f6fd7a1e22059e83bb4d1");
  assert(!omemoDecryptMessage(decrypted, &n, payload, sizeof(omemoKeyPayload), encrypted, 16));
  assert(n == 9);
  assert(!memcmp("plaintext", decrypted, 9));
#endif
}

// user is either a or b
#define Send(user, id) do { \
    assert(!omemoRandom(messages[id].payload, sizeof(omemoKeyPayload))); \
    assert(!omemoEncryptKey(&session##user, &store##user, &messages[id].msg, messages[id].payload)); \
  } while (0)

#define Recv(user, id, isprekey) do { \
    omemoKeyPayload dec; \
    assert(!omemoDecryptKey(&session##user, &store##user, dec, isprekey, messages[id].msg.p, messages[id].msg.n)); \
    assert(!memcmp(messages[id].payload, dec, sizeof(omemoKeyPayload))); \
  } while (0);

#define MKSKIPPEDN 10
struct omemoMessageKey mkskipped[MKSKIPPEDN];
int mkskippedi;

static void RemoveSkippedKey(int i) {
  size_t n = MKSKIPPEDN - i - 1;
  memmove(mkskipped + i, mkskipped + i + 1,
          n * sizeof(*mkskipped));
  mkskippedi--;
}

int omemoLoadMessageKey(struct omemoSession *, struct omemoMessageKey *k) {
  for (int i = 0; i < mkskippedi; i++) {
    struct omemoMessageKey *sk = &mkskipped[i];
    if (k->nr == sk->nr && !memcmp(k->dh, sk->dh, 32)) {
      memcpy(k->mk, sk->mk, 32);
      RemoveSkippedKey(i);
      return 0;
    }
  }
  return 1;
}

int omemoStoreMessageKey(struct omemoSession *, const struct omemoMessageKey *k, uint64_t) {
  if (mkskippedi >= MKSKIPPEDN)
    return OMEMO_EUSER;
  memcpy(&mkskipped[mkskippedi++], k, sizeof(*k));
  return 0;
}

static void TestSession() {
  struct {
    omemoKeyPayload payload;
    struct omemoKeyMessage msg;
  } messages[100];

  struct omemoStore storea, storeb;
  assert(!omemoSetupStore(&storea));
  assert(!omemoSetupStore(&storeb));

  struct omemoBundle bundleb;
  ParseBundle(&bundleb, &storeb);

  struct omemoSession sessiona, sessionb;
  memset(&sessiona, 0, sizeof(sessiona));
  memset(&sessionb, 0, sizeof(sessionb));
  assert(omemoInitFromBundle(&sessiona, &storea, &bundleb) == 0);

  Send(a, 0);
  Recv(b, 0, true);

  Send(b, 1);
  Recv(a, 1, false);

  Send(b, 2);
  Recv(a, 2, false);

  Send(b, 3);
  Send(b, 4);

  assert(mkskippedi == 0);
  Recv(a, 4, false);

  assert(mkskippedi == 1);
  Recv(a, 3, false);
  assert(mkskippedi == 0);

  memset(mkskipped, 0, sizeof(mkskipped));
  mkskippedi = 0;
}

// Test session built by Gajim
static void TestReceive() {
#ifndef OMEMO2
  struct omemoSession session;
  memset(&session, 0, sizeof(session));
  struct omemoStore store;
  memset(&store, 0, sizeof(struct omemoStore));
  store.init = true;
  store.prekeys[55].id = 56;
  CopyHex(store.prekeys[55].kp.pub, "c0a2e2216d40765490501fcf8d31892c1a4cf60ed880ae3422daa767c430916b");
  CopyHex(store.prekeys[55].kp.prv, "e8f9420a195d93f6d4acf9a5d92748aebd235bcd7648b19849882d96f8fdcf41");
  CopyHex(store.identity.pub, "4e261cb22646a9ed75a8cfa194452fd20a320634b985d45084f0d5c6fc08cc4a");
  CopyHex(store.identity.prv, "301defea859e6e440ef4a77d975c9a9590c59cddd275547adc59bf2c1d088d47");
  store.cursignedprekey.id = 1;
  CopyHex(store.cursignedprekey.kp.pub, "0a90c1ea3558b15625ad78e20861a39b1f30ca1c425a0e50557b0868821c661f");
  CopyHex(store.cursignedprekey.kp.prv, "805eb8a8982b4206d0bec56bd1e861141f2c1b48386fa35ee7231834be1dd478");
  CopyHex(store.cursignedprekey.sig, "0fa2490e4899a3da85a94093fb27e97f15d05e99bab361c9a4ca388bec6685c61d96241c0020c101854388fd41e8932a7e4fba37bc454a21a6bcc037b0407808");
  omemoKeyPayload payload;
  uint8_t msg[180];
  CopyHex(msg,"33083812210508a21e22879385c9f5ea5ef0a50b993167659fbc0e90614365b9d0147ac8f1201a21057f1a8715095495c17552d720975d8405c38ed11bee9404bca19062d352a9c7082252330a2105e5bbca217d32f97f860ecd3c47df86f2a71eb8d2e387e31dd1f5f5349863b455100018002220a0bae4d6e5da28a1897fa3562cd4d24ee60bc9a5d4daf0f13646239bec36a2b4fd5aa1843e12d6f128f1eaa07b3001");
  assert(omemoDecryptKey(&session, &store, payload, true, msg, 164) == 0);
#endif
}

static void TestDeriveChainKey() {
#ifndef OMEMO2
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

  omemoKey myck, mymk;
  GetBaseMaterials(myck, mymk, seed);
  assert(!memcmp(ck, myck, 32));
  struct DeriveChainKeyOutput kdfout[1];
  assert(!DeriveKey(Zero32, mymk, HkdfInfoMessageKeys, kdfout));
  assert(!memcmp(mk, kdfout->cipher, 32));
  assert(!memcmp(mac, kdfout->mac, 32));
#endif
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

static int GetSharedSecretWithoutPreKey(omemoKey rk, omemoKey ck, bool isbob, const omemoKey ika, const omemoKey ska, const omemoKey ikb, const omemoKey spkb) {
  uint8_t secret[32*4] = {0}, salt[32];
  memset(secret, 0xff, 32);
  // When we are bob, we must swap the first two.
  assert(!DoX25519(secret+32, isbob ? ska : ika, isbob ? ikb : spkb));
  assert(!DoX25519(secret+64, isbob ? ika : ska, isbob ? spkb : ikb));
  assert(!DoX25519(secret+96, ska, spkb));
  memset(salt, 0, 32);
  uint8_t full[64];
  if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, 32, secret, sizeof(secret), "WhisperText", 11, full, 64) != 0)
    return OMEMO_ECRYPTO;
  memcpy(rk, full, 32);
  memcpy(ck, full+32, 32);
  return 0;
}

static void TestRatchet() {
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

  omemoKey rk, ck;
  assert(GetSharedSecretWithoutPreKey(rk, ck, false, aliceIdentityPrivate, aliceBasePrivate, bobIdentityPublic+1, bobSignedPreKeyPublic+1) == 0);
  // TODO: aliceBasePublic<->aliceBasePrivate is different in HACL*
  //assert(!memcmp(ck, receiverAndSenderChain, 32));

  assert(GetSharedSecretWithoutPreKey(rk, ck, true, bobIdentityPrivate, bobSignedPreKeyPrivate, aliceIdentityPublic+1, aliceBasePublic+1) == 0);
  assert(!memcmp(ck, receiverAndSenderChain, 32));
}

static void TestSerialization() {
  struct omemoStore storea, storeb;
  assert(!omemoSetupStore(&storea));
  assert(!omemoSetupStore(&storeb));

  struct omemoBundle bundleb;
  ParseBundle(&bundleb, &storeb);

  struct omemoSession sessiona, sessionb;
  memset(&sessiona, 0, sizeof(sessiona));
  memset(&sessionb, 0, sizeof(sessionb));
  assert(omemoInitFromBundle(&sessiona, &storea, &bundleb) == 0);

  size_t n = omemoGetSerializedStoreSize(&storea);
  uint8_t *buf = malloc(n);
  assert(buf);
  omemoSerializeStore(buf, &storea);
  memset(&storeb, 0, sizeof(storeb));
  assert(!omemoDeserializeStore(buf, n, &storeb));
  assert(!memcmp(&storea, &storeb, sizeof(struct omemoStore)));

  struct omemoSession tmpsession;
  memset(&tmpsession, 0, sizeof(tmpsession));
  n = omemoGetSerializedSessionSize(&sessiona);
  uint8_t *buf2 = malloc(n);
  assert(buf2);
  omemoSerializeSession(buf2, &sessiona);
  assert(!omemoDeserializeSession(buf2, n, &tmpsession));
  assert(!memcmp(&tmpsession, &sessiona, sizeof(struct omemoSession)));
}

static void TestSessionIntegration() {
  struct omemoSession session;
  struct omemoStore store;
  memset(&session, 0, sizeof(session));
  uint8_t buf[1000];
  omemoDeserializeStore(store_inc, store_inc_len, &store);
  FILE *f = fopen("o/msg.bin", "r");
  assert(f);
  int n = fread(buf, 1, 1000, f);
  assert(n > 0);
  fclose(f);

  omemoKeyPayload payload;
  assert(!omemoDecryptKey(&session, &store, payload, true, buf, n));

  omemoKeyPayload exp;
#ifdef OMEMO2
  memset(exp,    0x55, 32);
  memset(exp+32, 0xaa, 16);
#else
  memset(exp,    0x55, 16);
  memset(exp+16, 0xaa, 16);
#endif
  assert(!memcmp(exp, payload, sizeof(exp)));

  memset(payload, 0xcc, sizeof(payload));
  struct omemoKeyMessage msg;
  assert(!omemoEncryptKey(&session, &store, &msg, payload));
  f = fopen("o/resp.bin", "w");
  assert(f);
  assert(fwrite(msg.p, 1, msg.n, f) == msg.n);
  fclose(f);
}

#define RunTest(t)                                                     \
  do {                                                                 \
    puts("\e[34mRunning test " #t "\e[0m");                            \
    t();                                                               \
    puts("\e[32mFinished test " #t "\e[0m");                           \
  } while (0)

int main() {
  // TODO: tests TestCurve25519, TestSignature, TestSession and
  // TestSerialization are slow because they use the slow
  // edsign_sign_modified and edsign_verify.
  RunTest(TestParseProtobuf);
  RunTest(TestFormatProtobuf);
  RunTest(TestFormattings);
  RunTest(TestEncryptSize);
  RunTest(TestProtobufPrekey);
  RunTest(TestCurve25519);
  RunTest(TestRotate);
  RunTest(TestSignature);
  RunTest(TestSignModified);
  RunTest(TestKeyConversions);
  RunTest(TestEncryption);
  RunTest(TestHkdf);
  RunTest(TestRatchet);
  RunTest(TestDeriveChainKey);
  RunTest(TestSerialization);
  RunTest(TestSessionIntegration);
  RunTest(TestReceive);
  RunTest(TestSession);
  puts("All tests succeeded");
}
