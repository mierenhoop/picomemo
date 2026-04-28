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

#ifndef OMEMO0_H_
#define OMEMO0_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef OMEMO0_EXPORT
#define OMEMO0_EXPORT
#endif

#define OMEMO0_NUMPREKEYS 100

#define OMEMO0_EPROTOBUF (-1)
#define OMEMO0_ECRYPTO   (-2)
#define OMEMO0_ECORRUPT  (-3)
#define OMEMO0_EPARAM    (-4)
#define OMEMO0_ESTATE    (-5)
#define OMEMO0_EKEYGONE  (-6)
#define OMEMO0_ESTORE    (-7)
#define OMEMO0_EUSER     (-8)
#define OMEMO0_ERANDOM   (-9)



#define OMEMO0_KEYSIZE                        32
#define OMEMO0_INTERNAL_PAYLOAD_MAXPADDEDSIZE 48
#define OMEMO0_INTERNAL_HEADER_MAXSIZE        (1 + 35 + 2 * 6 + 2)
#define OMEMO0_INTERNAL_FULLMSG_MAXSIZE                                 \
  (OMEMO0_INTERNAL_HEADER_MAXSIZE + OMEMO0_INTERNAL_PAYLOAD_MAXPADDEDSIZE)
#define OMEMO0_INTERNAL_PREKEYHEADER_MAXSIZE (1 + 2 + 2 * 6 + 35 * 2 + 2)
#define OMEMO0_INTERNAL_ENCRYPTED_MAXSIZE                               \
  (OMEMO0_INTERNAL_FULLMSG_MAXSIZE + 8)


typedef uint8_t omemo0Key[32];

typedef uint8_t omemo0SerializedKey[1 + 32];
typedef uint8_t omemo0CurveSignature[64];

struct omemo0KeyPair {
  omemo0Key prv;
  omemo0Key pub;
};

struct omemo0PreKey {
  uint32_t id;
  struct omemo0KeyPair kp;
};

struct omemo0SignedPreKey {
  uint32_t id;
  struct omemo0KeyPair kp;
  omemo0CurveSignature sig;
};

struct omemo0MessageKey {
  uint32_t nr;
  omemo0Key dh;
  omemo0Key mk;
};

struct omemo0State {
  struct omemo0KeyPair dhs;
  omemo0Key dhr;
  omemo0Key rk, cks, ckr;
  uint32_t ns, nr, pn;
};

struct omemo0KeyMessage {
  uint8_t p[OMEMO0_INTERNAL_PREKEYHEADER_MAXSIZE +
            OMEMO0_INTERNAL_ENCRYPTED_MAXSIZE];
  size_t n;
  bool isprekey;
};

struct omemo0Store {
  bool init;
  struct omemo0KeyPair identity;
  struct omemo0SignedPreKey cursignedprekey, prevsignedprekey;
  struct omemo0PreKey prekeys[OMEMO0_NUMPREKEYS];
  uint32_t pkcounter;
};

struct omemo0Session {
  int init;
  omemo0Key identity;
  omemo0Key remoteidentity;
  struct omemo0State state;
  omemo0Key usedek;
  uint32_t usedpk_id, usedspk_id;
};

typedef int (*omemo0LoadMessageKeyCallback)(struct omemo0Session *,
                                           struct omemo0MessageKey *sk);

typedef int (*omemo0StoreMessageKeyCallback)(
    struct omemo0Session *,
    const struct omemo0MessageKey *,
    uint64_t n);

typedef int (*omemo0RandomCallback)(void *p, size_t n);

int omemo0LoadMessageKey(struct omemo0Session *s,
                        struct omemo0MessageKey *sk);

int omemo0StoreMessageKey(struct omemo0Session *s,
                         const struct omemo0MessageKey *sk,
                         uint64_t n);

int omemo0Random(void *p, size_t n);

/**
 * Set global callbacks for storing/loading skipped message keys and
 * random generation.
 */
OMEMO0_EXPORT void omemo0SetCallbacks(omemo0LoadMessageKeyCallback,
                                    omemo0StoreMessageKeyCallback,
                                    omemo0RandomCallback);

/**
 * Serialize a raw public key into the OMEMO public key format.
 */
OMEMO0_EXPORT void omemo0SerializeKey(omemo0SerializedKey k,
                                    const omemo0Key pub);

/**
 * Generate a new store for an OMEMO device.
 *
 * @returns 0 or OMEMO0_E*
 */
OMEMO0_EXPORT int omemo0SetupStore(struct omemo0Store *store);

/**
 * Refill all removed prekeys in store.
 *
 * @returns 0 or OMEMO0_ECRYPTO
 */
OMEMO0_EXPORT int omemo0RefillPreKeys(struct omemo0Store *store);

/**
 * Rotate signed prekey in store.
 *
 * Retains the previous signed prekey for one rotation.
 *
 * @returns 0 or OMEMO0_ECRYPTO
 */
OMEMO0_EXPORT int omemo0RotateSignedPreKey(struct omemo0Store *store);

/**
 * @returns size of buffer required for omemo0SerializeStore
 */
OMEMO0_EXPORT size_t
omemo0GetSerializedStoreSize(const struct omemo0Store *store);

/**
 * @param d buffer with capacity returned by
 * omemo0GetSerializedStoreSize()
 */
OMEMO0_EXPORT void omemo0SerializeStore(uint8_t *d,
                                      const struct omemo0Store *store);

/**
 * @returns 0 or OMEMO0_E*
 */
OMEMO0_EXPORT int omemo0DeserializeStore(const uint8_t *p, size_t n,
                                       struct omemo0Store *store);
/**
 * @returns size of buffer required for omemo0SerializeSession
 */
OMEMO0_EXPORT size_t
omemo0GetSerializedSessionSize(const struct omemo0Session *session);

/**
 * @param d buffer with capacity returned by
 * omemo0GetSerializedSessionSize()
 */
OMEMO0_EXPORT void
omemo0SerializeSession(uint8_t *d, const struct omemo0Session *session);

/**
 * @param session must be initialized with omemo0SetupSession
 * @return 0 or OMEMO0_EPROTOBUF
 */
OMEMO0_EXPORT int omemo0DeserializeSession(const uint8_t *p, size_t n,
                                         struct omemo0Session *session);

/**
 * Initiate OMEMO session with retrieved bundle.
 *
 * @returns 0 or OMEMO0_E*
 */
OMEMO0_EXPORT int omemo0InitiateSession(struct omemo0Session *session,
                                      const struct omemo0Store *store,
                                      const omemo0CurveSignature spks,
                                      const omemo0SerializedKey spk,
                                      const omemo0SerializedKey ik,
                                      const omemo0SerializedKey pk,
                                      uint32_t spk_id, uint32_t pk_id);

/**
 * Encrypt message encryption key payload for a specific recipient.
 *
 * @returns 0 or OMEMO0_E*
 */
OMEMO0_EXPORT int omemo0EncryptKey(struct omemo0Session *session,
                                 struct omemo0KeyMessage *msg,
                                 const uint8_t *key, size_t keyn);
/**
 * Decrypt message encryption key payload for a specific recipient.
 *
 * If a prekey is used, it will be stored in session->usedpk_id, which
 * should be removed from the store and bundle after catching up with
 * all other messages. Remove by iterating over store->prekeys and
 * zeroing the omemo0PreKey structure where id == store->usedpk_id.
 *
 * If session->state.nr >= 53 you should send an empty message back to
 * advance the ratchet.
 *
 * @returns 0 or OMEMO0_E*
 */
OMEMO0_EXPORT int omemo0DecryptKey(struct omemo0Session *session,
                                 const struct omemo0Store *store,
                                 uint8_t *key, size_t *keyn,
                                 bool isprekey, const uint8_t *msg,
                                 size_t msgn);

/**
 * Create a heartbeat message if the ratchet counter is too high.
 *
 * This function can should be called after every omemo0DecryptKey(). It
 * checks whether the counter is too high. When it is, it will fill
 * the omemo0KeyMessage with a newly encrypted key which should be sent
 * afterwards. To check whether a heartbeat msg was made, check if
 * msg->n > 0.
 *
 * @returns 0 or OMEMO0_E*
 */
OMEMO0_EXPORT int omemo0Heartbeat(struct omemo0Session *session,
                                const struct omemo0Store *store,
                                struct omemo0KeyMessage *msg);


/**
 * Encrypt message which will be stored in the <payload> element.
 *
 * @param key (out) will contain the encryption key
 * @param n is the size of the buffer in d and s
 *
 * @returns 0 or OMEMO0_E*
 */
OMEMO0_EXPORT int omemo0EncryptMessage(uint8_t *d, uint8_t key[32],
                                     uint8_t iv[12], const uint8_t *s,
                                     size_t n);


/**
 * Decrypt message taken from the <payload> element.
 *
 * @param key is the decrypted key of the omemo0KeyMessage
 * @param keyn is the size of key, some clients might make the tag
 * larger than 16 bytes
 * @param n is the size of the buffer in d and s
 *
 * @returns 0 or OMEMO0_E*
 */
OMEMO0_EXPORT int omemo0DecryptMessage(uint8_t *d, const uint8_t *key,
                                     size_t keyn, const uint8_t iv[12],
                                     const uint8_t *s, size_t n);

#endif
