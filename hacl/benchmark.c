#include <stdint.h>

#include <sys/random.h>
#include "Hacl_Curve25519_51.h"
#include "Hacl_Ed25519.h"
#include "../c25519.h"

int main() {
  uint8_t prv[32], pub[32], sig[64];
  const char *msg = "fjdakfjadkffjkadjfkadfjja";
  getrandom(prv, 32, 0);
  ed25519_prepare(prv);
  Hacl_Ed25519_secret_to_public(pub, prv);
  Hacl_Ed25519_sign(sig, prv, sizeof(msg), msg);
  for (int i = 0; i < 1000; i++)
#ifdef HACL
    //Hacl_Ed25519_secret_to_public(pub, prv);
    //Hacl_Curve25519_51_secret_to_public(pub, prv);
    //Hacl_Ed25519_sign(sig, prv, sizeof(msg), msg);
    Hacl_Ed25519_verify(pub, sizeof(msg), msg, sig);
#else
    //edsign_sec_to_pub(pub, prv);
    //c25519_smult(pub, c25519_base_x, prv);
    //edsign_sign(sig, pub, prv, msg, sizeof(msg));
    edsign_verify(sig, pub, msg, sizeof(msg));
#endif
}

// c25519 smult/sec_to_pub  ~90x   ~.05 / 4.4
// ed25519 sec_to_pub       ~90x   ~.05 / 4.4
// sign                     ~50x   ~.08 / 3.9
// verify                   ~110x  ~.08 / 9.8
