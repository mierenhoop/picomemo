#include <stdint.h>
#include <assert.h>

#include <sys/random.h>
#include "Hacl_Curve25519_51.h"
#include "Hacl_Ed25519.h"
#include "hacl.h"
#include "internal/Hacl_Ed25519.h"
#include "internal/Hacl_Bignum25519_51.h"
#include "../c25519.h"

static void ConvertCurvePrvToEdPub(uint8_t ed[32], const uint8_t prv[32]) {
  struct ed25519_pt p;
  ed25519_smult(&p, &ed25519_base, prv);
  uint8_t x[F25519_SIZE];
  uint8_t y[F25519_SIZE];
  ed25519_unproject(x, y, &p);
  ed25519_pack(ed, x, y);
}

void Calc(uint8_t sig[64], uint8_t prv[32], uint8_t msg[33]) {
  uint8_t ed[32];
  //uint8_t msgbuf[33+64];
  //int sign = 0;
  //memcpy(msgbuf, msg, 33);
  //memset(msgbuf+33, 0xcc, 64);
  //ConvertCurvePrvToEdPub(ed, prv);
  //sign = ed[31] & 0x80;
  //edsign_sign_modified(sig, ed, prv, msgbuf, 33);
  //sig[63] &= 0x7f;
  //sig[63] |= sign;
  //for (int i=0;i<64;i++) printf("%02x",sig[i]);
  //puts("");
  //Hacl_Ed25519_sign_modified(sig, ed, prv, msgbuf, 33);
  //for (int i=0;i<64;i++) printf("%02x",sig[i]);
  //puts("");
}

int main() {
  uint8_t prv[32], pub[32], sig[64], edx[32], edy[32];
  const char *msg = "fjdakfjadkffjkadjfkadfjja";
  getrandom(prv, 32, 0);
  c25519_prepare(prv);
  c25519_smult(pub, c25519_base_x, prv);
  //ed25519_prepare(prv);
  //Hacl_Ed25519_secret_to_public(pub, prv);
  //ed25519_try_unpack(edx, edy, pub);
  //Hacl_Ed25519_sign(sig, prv, sizeof(msg), msg);
  for (int i = 0; i < 1000; i++)
#ifdef HACL
    //Hacl_Ed25519_secret_to_public(pub, prv);
    //Hacl_Curve25519_51_secret_to_public(pub, prv);
    //Hacl_Ed25519_sign(sig, prv, sizeof(msg), msg);
    //Hacl_Ed25519_verify(pub, sizeof(msg), msg, sig);
    //TryUnpackY(edy, pub);
    //Mx2Ey(edy, pub);
    //E2M(pub, edy);
    Hacl_Ed25519_pub_from_Curve25519_priv(pub, prv);
    //;
#else
    //edsign_sec_to_pub(pub, prv);
    //c25519_smult(pub, c25519_base_x, prv);
    //edsign_sign(sig, pub, prv, msg, sizeof(msg));
    //edsign_verify(sig, pub, msg, sizeof(msg));
    //morph25519_e2m(pub, edy);
    //ed25519_try_unpack(edx, edy, pub);
    //morph25519_mx2ey(edy, pub);
    //expand_key(sig, prv);
    //morph25519_mx2ey(edy, pub);
    //morph25519_e2m(pub, edy);
    ConvertCurvePrvToEdPub(pub, prv);
    //;
#endif

  //ed25519_try_unpack(edx, edy, pub);
  //morph25519_e2m(pub, edy);
  //for (int i = 0; i < 32; i++) printf("%02x", pub[i]);
  //puts("");
  //E2M(pub, edy);
  //for (int i = 0; i < 32; i++) printf("%02x", pub[i]);
  //puts("");
  //uint8_t y[32];
  //TryUnpackY(y, pub);
  //for (int i = 0; i < 32; i++) printf("%02x", y[i]);
  //puts("");
  //uint8_t ey[32];
  //morph25519_mx2ey(ey, pub);
  //for (int i = 0; i < 32; i++) printf("%02x", ey[i]);
  //puts("");
  //Mx2Ey(ey, pub);
  //for (int i = 0; i < 32; i++) printf("%02x", ey[i]);
  //puts("");
  uint8_t serkey[33];
  memset(serkey, 0xaa, 33);
  Calc(sig, prv, serkey);
  //ConvertCurvePrvToEdPub();
}


//                                1000 iters
// c25519 smult/sec_to_pub  ~90x   ~.05 / 4.4
// ed25519 sec_to_pub       ~90x   ~.05 / 4.4
// sign                     ~50x   ~.08 / 3.9
// verify                   ~110x  ~.08 / 9.8
// morph25519_e2m                  ~.01 / 0.4
// morph25519_mx2ey                ~.01 / 0.4
// ed25519_try_unpack              ~.01 / 0.7
// expand_key/secret_expand               0.0
// c prv -> ed pub                 ~.05 / 4.2
