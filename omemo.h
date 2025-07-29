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

#ifndef OMEMO_H_
#define OMEMO_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef OMEMO_EXPORT
#define OMEMO_EXPORT
#endif

#define OMEMO_NUMPREKEYS 100

#define OMEMO_EPROTOBUF (-1)
#define OMEMO_ECRYPTO   (-2)
#define OMEMO_ECORRUPT  (-3)
#define OMEMO_EPARAM    (-4)
#define OMEMO_ESTATE    (-5)
#define OMEMO_EKEYGONE  (-6)
#define OMEMO_EUSER     (-7)

#ifdef OMEMO2

#define OMEMO_KEYSIZE                        48
#define OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE 64
#define OMEMO_INTERNAL_HEADER_MAXSIZE        (2 * 6 + 34 + 2)
#define OMEMO_INTERNAL_FULLMSG_MAXSIZE                                 \
  (OMEMO_INTERNAL_HEADER_MAXSIZE + OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE)
#define OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE (6 * 2 + 34 * 2 + 3)
#define OMEMO_INTERNAL_ENCRYPTED_MAXSIZE                               \
  (2 + 16 + 2 + OMEMO_INTERNAL_FULLMSG_MAXSIZE)

#else

#define OMEMO_KEYSIZE                        32
#define OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE 48
#define OMEMO_INTERNAL_HEADER_MAXSIZE        (1 + 35 + 2 * 6 + 2)
#define OMEMO_INTERNAL_FULLMSG_MAXSIZE                                 \
  (OMEMO_INTERNAL_HEADER_MAXSIZE + OMEMO_INTERNAL_PAYLOAD_MAXPADDEDSIZE)
#define OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE (1 + 2 + 2 * 6 + 35 * 2 + 2)
#define OMEMO_INTERNAL_ENCRYPTED_MAXSIZE                               \
  (OMEMO_INTERNAL_FULLMSG_MAXSIZE + 8)

#endif

#define OMEMO_MAXPAYLOAD OMEMO_KEYSIZE

typedef uint8_t omemoKey[32];
#ifdef OMEMO2
typedef uint8_t omemoSerializedKey[32];
#else
typedef uint8_t omemoSerializedKey[1 + 32];
#endif
typedef uint8_t omemoCurveSignature[64];

struct omemoKeyPair {
  omemoKey prv;
  omemoKey pub;
};

struct omemoPreKey {
  uint32_t id;
  struct omemoKeyPair kp;
};

struct omemoSignedPreKey {
  uint32_t id;
  struct omemoKeyPair kp;
  omemoCurveSignature sig;
};

struct omemoMessageKey {
  uint32_t nr;
  omemoKey dh;
  omemoKey mk;
};

struct omemoState {
  struct omemoKeyPair dhs;
  omemoKey dhr;
  omemoKey rk, cks, ckr;
  uint32_t ns, nr, pn;
};

struct omemoKeyMessage {
  uint8_t p[OMEMO_INTERNAL_PREKEYHEADER_MAXSIZE +
            OMEMO_INTERNAL_ENCRYPTED_MAXSIZE];
  size_t n;
  bool isprekey;
};

// As the spec notes, a spk should be kept for one more rotation.
// If prevsignedprekey doesn't exist, its id is 0. Therefore a valid id
// is always >= 1; pkcounter is the id of the most recently generated
// prekey.
struct omemoStore {
  bool init;
  struct omemoKeyPair identity;
  struct omemoSignedPreKey cursignedprekey, prevsignedprekey;
  struct omemoPreKey prekeys[OMEMO_NUMPREKEYS];
  uint32_t pkcounter;
};

struct omemoSession {
  int init;
  omemoKey remoteidentity;
  struct omemoState state;
  omemoKey usedek;
  uint32_t usedpk_id, usedspk_id;
};

/**
 * User supplied function.
 *
 * To pass userdata to this callback, it is recommended to wrap
 * omemoSession within another struct and appending user data fields to
 * the new struct.
 *
 * @param sk has the nr and dh field filled, use them to look up the mk
 * and copy it to st->mk
 * @returns 0 when found or 1 when not found or OMEMO_E*
 */
OMEMO_EXPORT int omemoLoadMessageKey(struct omemoSession *,
                                     struct omemoMessageKey *sk);

/**
 * User supplied function.
 *
 * When n > max skipped keys, the implementation of this function should
 * return an error. omemoStoreMessageKey will be called n-1 more times.
 *
 * @see omemoLoadMessageKey()
 *
 * @param n amount of keys to be skipped in total
 */
OMEMO_EXPORT int omemoStoreMessageKey(struct omemoSession *,
                                      const struct omemoMessageKey *,
                                      uint64_t n);

/**
 * Unimplemented random function.
 *
 * This function should be externally implemented.
 *
 * @param p points to the to-be-filled array
 * @param n is the amount of random bytes which should be generated in p
 * @returns 0 if successful, anything else otherwise
 */
OMEMO_EXPORT int omemoRandom(void *p, size_t n);

/**
 * Serialize a raw public key into the OMEMO public key format.
 */
OMEMO_EXPORT void omemoSerializeKey(omemoSerializedKey k,
                                    const omemoKey pub);

/**
 * Generate a new store for an OMEMO device.
 */
OMEMO_EXPORT int omemoSetupStore(struct omemoStore *store);

/**
 * Refill prekeys in store.
 *
 * @returns 0 or OMEMO_ECRYPTO
 */
OMEMO_EXPORT int omemoRefillPreKeys(struct omemoStore *store);

OMEMO_EXPORT int omemoRotateSignedPreKey(struct omemoStore *store);

OMEMO_EXPORT size_t
omemoGetSerializedStoreSize(const struct omemoStore *store);
OMEMO_EXPORT void omemoSerializeStore(uint8_t *d,
                                      const struct omemoStore *store);
OMEMO_EXPORT int omemoDeserializeStore(const char *p, size_t n,
                                       struct omemoStore *store);
OMEMO_EXPORT size_t
omemoGetSerializedSessionSize(const struct omemoSession *session);
OMEMO_EXPORT void
omemoSerializeSession(uint8_t *p, const struct omemoSession *session);

/**
 * @param session must be initialized with omemoSetupSession
 * @return 0 or OMEMO_EPROTOBUF
 */
OMEMO_EXPORT int omemoDeserializeSession(const char *p, size_t n,
                                         struct omemoSession *session);

/**
 * Initialize OMEMO session from retrieved bundle.
 */
OMEMO_EXPORT int omemoInitiateSession(struct omemoSession *session,
                                      const struct omemoStore *store,
                                      const omemoCurveSignature spks,
                                      const omemoSerializedKey spk,
                                      const omemoSerializedKey ik,
                                      const omemoSerializedKey pk,
                                      uint32_t spk_id, uint32_t pk_id);

/**
 * Encrypt message encryption key payload for a specific recipient.
 */
OMEMO_EXPORT int omemoEncryptKey(struct omemoSession *session,
                                 const struct omemoStore *store,
                                 struct omemoKeyMessage *msg,
                                 const uint8_t *payload, size_t pn);
/**
 * Decrypt message encryption key payload for a specific recipient.
 *
 * If a prekey is used, it will be stored in session->usedpk_id, which
 * should be removed from the store and bundle after catching up with
 * all other messages. Remove by iterating over store->prekeys and
 * zeroing the omemoPreKey structure where id == store->usedpk_id.
 *
 * If session->state.nr >= 53 you should send an empty message back to
 * advance the ratchet.
 */
OMEMO_EXPORT int omemoDecryptKey(struct omemoSession *session,
                                 struct omemoStore *store,
                                 uint8_t *payload, size_t *pn,
                                 bool isprekey, const uint8_t *msg,
                                 size_t msgn);

#ifdef OMEMO2
#define omemoGetMessagePadSize(n) (16 - (n % 16))
/**
 * Encrypt message which will be stored in the <payload> element.
 *
 * @param payload (out) will contain the encrypted
 * @param s is a mutable buffer containing the plaintext message with
 * `omemoGetMessagePadSize(n)` amount of bytes reserved at the end
 * @param d is the destination buffer that is the same size as s
 * @param n is the original message size
 */
OMEMO_EXPORT int omemoEncryptMessage(uint8_t *d, uint8_t payload[48],
                                     uint8_t *s, size_t n);
#else
/**
 * Encrypt message which will be stored in the <payload> element.
 *
 * @param payload (out) will contain the encrypted
 * @param n is the size of the buffer in d and s
 */
OMEMO_EXPORT int omemoEncryptMessage(uint8_t *d, uint8_t payload[32],
                                     uint8_t iv[12], const uint8_t *s,
                                     size_t n);
#endif

#ifdef OMEMO2
/**
 * Decrypt message taken from the <payload> element.
 *
 * @param payload is the decrypted payload of the omemoKeyMessage
 * @param pn is the size of payload
 * @param n is the size of the buffer in d and s
 */
OMEMO_EXPORT int omemoDecryptMessage(uint8_t *d, size_t *outn,
                                     const uint8_t *payload, size_t pn,
                                     const uint8_t *s, size_t n);
#else
/**
 * Decrypt message taken from the <payload> element.
 *
 * @param payload is the decrypted payload of the omemoKeyMessage
 * @param pn is the size of payload, some clients might make the tag
 * larger than 16 bytes
 * @param n is the size of the buffer in d and s
 */
OMEMO_EXPORT int omemoDecryptMessage(uint8_t *d, const uint8_t *payload,
                                     size_t pn, const uint8_t iv[12],
                                     const uint8_t *s, size_t n);
#endif

#endif
