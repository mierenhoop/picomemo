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

#ifndef OMEMO2_H_
#define OMEMO2_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef OMEMO2_EXPORT
#define OMEMO2_EXPORT
#endif

#define OMEMO2_NUMPREKEYS 100

#define OMEMO2_EPROTOBUF (-1)
#define OMEMO2_ECRYPTO   (-2)
#define OMEMO2_ECORRUPT  (-3)
#define OMEMO2_EPARAM    (-4)
#define OMEMO2_ESTATE    (-5)
#define OMEMO2_EKEYGONE  (-6)
#define OMEMO2_ESTORE    (-7)
#define OMEMO2_EUSER     (-8)
#define OMEMO2_ERANDOM   (-9)


#define OMEMO2_KEYSIZE                        48
#define OMEMO2_INTERNAL_PAYLOAD_MAXPADDEDSIZE 64
#define OMEMO2_INTERNAL_HEADER_MAXSIZE        (2 * 6 + 34 + 2)
#define OMEMO2_INTERNAL_FULLMSG_MAXSIZE                                 \
  (OMEMO2_INTERNAL_HEADER_MAXSIZE + OMEMO2_INTERNAL_PAYLOAD_MAXPADDEDSIZE)
#define OMEMO2_INTERNAL_PREKEYHEADER_MAXSIZE (6 * 2 + 34 * 2 + 3)
#define OMEMO2_INTERNAL_ENCRYPTED_MAXSIZE                               \
  (2 + 16 + 2 + OMEMO2_INTERNAL_FULLMSG_MAXSIZE)


typedef uint8_t omemo2Key[32];
typedef uint8_t omemo2SerializedKey[32];
typedef uint8_t omemo2CurveSignature[64];

struct omemo2KeyPair {
  omemo2Key prv;
  omemo2Key pub;
};

struct omemo2PreKey {
  uint32_t id;
  struct omemo2KeyPair kp;
};

struct omemo2SignedPreKey {
  uint32_t id;
  struct omemo2KeyPair kp;
  omemo2CurveSignature sig;
};

struct omemo2MessageKey {
  uint32_t nr;
  omemo2Key dh;
  omemo2Key mk;
};

struct omemo2State {
  struct omemo2KeyPair dhs;
  omemo2Key dhr;
  omemo2Key rk, cks, ckr;
  uint32_t ns, nr, pn;
};

struct omemo2KeyMessage {
  uint8_t p[OMEMO2_INTERNAL_PREKEYHEADER_MAXSIZE +
            OMEMO2_INTERNAL_ENCRYPTED_MAXSIZE];
  size_t n;
  bool isprekey;
};

struct omemo2Store {
  bool init;
  struct omemo2KeyPair identity;
  struct omemo2SignedPreKey cursignedprekey, prevsignedprekey;
  struct omemo2PreKey prekeys[OMEMO2_NUMPREKEYS];
  uint32_t pkcounter;
};

struct omemo2Session {
  int init;
  omemo2Key identity;
  omemo2Key remoteidentity;
  struct omemo2State state;
  omemo2Key usedek;
  uint32_t usedpk_id, usedspk_id;
};

typedef int (*omemo2LoadMessageKeyCallback)(struct omemo2Session *,
                                           struct omemo2MessageKey *sk);

typedef int (*omemo2StoreMessageKeyCallback)(
    struct omemo2Session *,
    const struct omemo2MessageKey *,
    uint64_t n);

typedef int (*omemo2RandomCallback)(void *p, size_t n);

int omemo2LoadMessageKey(struct omemo2Session *s,
                        struct omemo2MessageKey *sk);

int omemo2StoreMessageKey(struct omemo2Session *s,
                         const struct omemo2MessageKey *sk,
                         uint64_t n);

int omemo2Random(void *p, size_t n);

/**
 * Set global callbacks for storing/loading skipped message keys and
 * random generation.
 */
OMEMO2_EXPORT void omemo2SetCallbacks(omemo2LoadMessageKeyCallback,
                                    omemo2StoreMessageKeyCallback,
                                    omemo2RandomCallback);

/**
 * Serialize a raw public key into the OMEMO public key format.
 */
OMEMO2_EXPORT void omemo2SerializeKey(omemo2SerializedKey k,
                                    const omemo2Key pub);

/**
 * Generate a new store for an OMEMO device.
 *
 * @returns 0 or OMEMO2_E*
 */
OMEMO2_EXPORT int omemo2SetupStore(struct omemo2Store *store);

/**
 * Refill all removed prekeys in store.
 *
 * @returns 0 or OMEMO2_ECRYPTO
 */
OMEMO2_EXPORT int omemo2RefillPreKeys(struct omemo2Store *store);

/**
 * Rotate signed prekey in store.
 *
 * Retains the previous signed prekey for one rotation.
 *
 * @returns 0 or OMEMO2_ECRYPTO
 */
OMEMO2_EXPORT int omemo2RotateSignedPreKey(struct omemo2Store *store);

/**
 * @returns size of buffer required for omemo2SerializeStore
 */
OMEMO2_EXPORT size_t
omemo2GetSerializedStoreSize(const struct omemo2Store *store);

/**
 * @param d buffer with capacity returned by
 * omemo2GetSerializedStoreSize()
 */
OMEMO2_EXPORT void omemo2SerializeStore(uint8_t *d,
                                      const struct omemo2Store *store);

/**
 * @returns 0 or OMEMO2_E*
 */
OMEMO2_EXPORT int omemo2DeserializeStore(const uint8_t *p, size_t n,
                                       struct omemo2Store *store);
/**
 * @returns size of buffer required for omemo2SerializeSession
 */
OMEMO2_EXPORT size_t
omemo2GetSerializedSessionSize(const struct omemo2Session *session);

/**
 * @param d buffer with capacity returned by
 * omemo2GetSerializedSessionSize()
 */
OMEMO2_EXPORT void
omemo2SerializeSession(uint8_t *d, const struct omemo2Session *session);

/**
 * @param session must be initialized with omemo2SetupSession
 * @return 0 or OMEMO2_EPROTOBUF
 */
OMEMO2_EXPORT int omemo2DeserializeSession(const uint8_t *p, size_t n,
                                         struct omemo2Session *session);

/**
 * Initiate OMEMO session with retrieved bundle.
 *
 * @returns 0 or OMEMO2_E*
 */
OMEMO2_EXPORT int omemo2InitiateSession(struct omemo2Session *session,
                                      const struct omemo2Store *store,
                                      const omemo2CurveSignature spks,
                                      const omemo2SerializedKey spk,
                                      const omemo2SerializedKey ik,
                                      const omemo2SerializedKey pk,
                                      uint32_t spk_id, uint32_t pk_id);

/**
 * Encrypt message encryption key payload for a specific recipient.
 *
 * @returns 0 or OMEMO2_E*
 */
OMEMO2_EXPORT int omemo2EncryptKey(struct omemo2Session *session,
                                 struct omemo2KeyMessage *msg,
                                 const uint8_t *key, size_t keyn);
/**
 * Decrypt message encryption key payload for a specific recipient.
 *
 * If a prekey is used, it will be stored in session->usedpk_id, which
 * should be removed from the store and bundle after catching up with
 * all other messages. Remove by iterating over store->prekeys and
 * zeroing the omemo2PreKey structure where id == store->usedpk_id.
 *
 * If session->state.nr >= 53 you should send an empty message back to
 * advance the ratchet.
 *
 * @returns 0 or OMEMO2_E*
 */
OMEMO2_EXPORT int omemo2DecryptKey(struct omemo2Session *session,
                                 const struct omemo2Store *store,
                                 uint8_t *key, size_t *keyn,
                                 bool isprekey, const uint8_t *msg,
                                 size_t msgn);

/**
 * Create a heartbeat message if the ratchet counter is too high.
 *
 * This function can should be called after every omemo2DecryptKey(). It
 * checks whether the counter is too high. When it is, it will fill
 * the omemo2KeyMessage with a newly encrypted key which should be sent
 * afterwards. To check whether a heartbeat msg was made, check if
 * msg->n > 0.
 *
 * @returns 0 or OMEMO2_E*
 */
OMEMO2_EXPORT int omemo2Heartbeat(struct omemo2Session *session,
                                const struct omemo2Store *store,
                                struct omemo2KeyMessage *msg);

#define omemo2GetMessagePadSize(n) (16 - (n % 16))
/**
 * Encrypt message which will be stored in the <payload> element.
 *
 * @param key (out) will contain the encryption key
 * @param s is a mutable buffer containing the plaintext message with
 * `omemo2GetMessagePadSize(n)` amount of bytes reserved at the end
 * @param d is the destination buffer that is the same size as s
 * @param n is the original message size
 *
 * @returns 0 or OMEMO2_E*
 */
OMEMO2_EXPORT int omemo2EncryptMessage(uint8_t *d, uint8_t key[48],
                                     uint8_t *s, size_t n);

/**
 * Decrypt message taken from the <payload> element.
 *
 * @param key is the decrypted key of the omemo2KeyMessage
 * @param keyn is the size of key
 * @param n is the size of the buffer in d and s
 *
 * @returns 0 or OMEMO2_E*
 */
OMEMO2_EXPORT int omemo2DecryptMessage(uint8_t *d, size_t *outn,
                                     const uint8_t *key, size_t keyn,
                                     const uint8_t *s, size_t n);

#endif
