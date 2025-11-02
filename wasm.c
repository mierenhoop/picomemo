#include <emscripten.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

// Declare them as static so that EMSCRIPTEN_KEEPALIVE does not apply.
// They are implemented in JS, which is faster and saves ~50K from the
// WASM binary.

static int omemoEncryptMessage(uint8_t *d, uint8_t payload[32],
                               uint8_t iv[12], const uint8_t *s,
                               size_t n);
static int omemoDecryptMessage(uint8_t *d, const uint8_t *payload,
                               size_t pn, const uint8_t iv[12],
                               const uint8_t *s, size_t n);

#define OMEMO_EXPORT EMSCRIPTEN_KEEPALIVE
#include "omemo.h"
#include "omemo.c"
#include "hacl.c"

#define EX EMSCRIPTEN_KEEPALIVE

int omemoRandom(void *p, size_t n) {
  EM_ASM({
    HEAPU8.set(crypto.getRandomValues(new Uint8Array($1)), $0)
  }, p, n);
  return 0;
}

// When omemoStoreMessageKey is called and !g_skipbuf then g_nskip will
// contain amount of to-be-stored keys and omemoDecryptKey will return
// with user error. Now the JS caller will allocate and set g_skipbuf
// with enough space to hold g_nskip keys.
EX uint64_t g_nskip;
EX struct omemoMessageKey *g_skipbuf;
EX uint64_t get_nskip() { return g_nskip; }
EX void set_skipbuf(void *p) { g_skipbuf = p; }

// When omemoLoadMessageKey is called g_loadkey is set to the key arg
// and return with user error. The JS caller will put the mk into
// g_loadkey and call omemoDecryptKey again.
EX bool g_triedload;
EX bool g_suppliedkey;
EX struct omemoMessageKey g_loadkey;

int omemoLoadMessageKey(struct omemoSession *s,
                                     struct omemoMessageKey *k) {
  if (!g_triedload) {
    g_loadkey.nr = k->nr;
    memcpy(g_loadkey.dh, k->dh, 32);
    return OMEMO_EUSER;
  }
  assert(k->nr == g_loadkey.nr);
  assert(!memcmp(k->dh, g_loadkey.dh, 32));
  if (g_suppliedkey) {
    memcpy(k->mk, g_loadkey.mk, 32);
    return 0;
  } else {
    return 1;
  }
}

int omemoStoreMessageKey(struct omemoSession *s,
                                      const struct omemoMessageKey *k,
                                      uint64_t n) {
  if (g_skipbuf) {
    memcpy(g_skipbuf+(g_nskip - n), k, sizeof(*k));
    return 0;
  } else {
    g_nskip = n;
    return OMEMO_EUSER;
  }
}

EX int get_sessionsize   (void) { return sizeof(struct omemoSession   ); }
EX int get_storesize     (void) { return sizeof(struct omemoStore     ); }
EX int get_messagekeysize(void) { return sizeof(struct omemoMessageKey); }
EX int get_keymessagesize(void) { return sizeof(struct omemoKeyMessage); }
EX int get_numprekeys    (void) { return OMEMO_NUMPREKEYS              ; }

EX uint8_t *get_mk_dh(struct omemoMessageKey *mk) { return mk->dh; }
EX uint8_t *get_mk_mk(struct omemoMessageKey *mk) { return mk->mk; }
EX uint32_t get_mk_nr(struct omemoMessageKey *mk) { return mk->nr; }
// For heartbeat
EX uint32_t get_session_nr(struct omemoSession *s) { return s->state.nr; }

EX uint8_t *get_km_p   (struct omemoKeyMessage *m) { return m->p       ; }
EX size_t   get_km_n   (struct omemoKeyMessage *m) { return m->n       ; }
EX bool     get_km_ispk(struct omemoKeyMessage *m) { return m->isprekey; }

EX uint8_t *get_store_ik    (struct omemoStore *s) { return s->identity.pub          ; }
EX uint8_t *get_store_spk   (struct omemoStore *s) { return s->cursignedprekey.kp.pub; }
EX uint8_t *get_store_spks  (struct omemoStore *s) { return s->cursignedprekey.sig   ; }
EX uint32_t get_store_spk_id(struct omemoStore *s) { return s->cursignedprekey.id    ; }

EX uint32_t get_store_pk_id(struct omemoStore *s, int i) { return s->prekeys[i].id; }
EX uint8_t *get_store_pk   (struct omemoStore *s, int i) { return s->prekeys[i].kp.pub; }

EX void set_mk_mk(struct omemoMessageKey *k, omemoKey mk) {
  memcpy(k->mk, mk, 32);
}
