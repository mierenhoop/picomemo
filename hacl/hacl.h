#ifndef HACL_H_
#define HACL_H_

// OMEMO Additions

void Hacl_Ed25519_sign_modified(
  uint8_t *signature,
  uint8_t *public_key,
  uint8_t *s,
  uint8_t *msg,
  uint32_t msg_len
);

void Hacl_Ed25519_pub_from_Curve25519_priv(
    uint8_t *pub,
    uint8_t *sec);

void Hacl_Ed25519_seed_to_pub_priv(
    uint8_t *public_key,
    uint8_t *private_key, uint8_t *seed);

bool Hacl_Ed25519_pub_to_Curve25519_pub(
    uint8_t *m,
    uint8_t *e);

void Hacl_Curve25519_pub_to_Ed25519_pub(
    uint8_t *e,
    uint8_t *m);

#endif
