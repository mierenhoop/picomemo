#include <emscripten.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#define OMEMO_EXPORT EMSCRIPTEN_KEEPALIVE
#include "omemo.h"
#include "omemo.c"
#include "hacl.c"

// rm -f o/test-omemo* && CFLAGS="-Os -s -flto -DOMEMO2 -s EXPORTED_FUNCTIONS=_malloc,_main" MBED_VENDOR=mbedtls emmake make o/test-omemo && ls -lah o/test-omemo.wasm
//
// TODO: implement En/DecryptMessage in js

extern int omemoJsLoadMessageKey(int,int,int,int);
extern int omemoJsStoreMessageKey(int,int,int,int,int);
extern void omemoJsRandom(int,int);

int omemoRandom(void *p, size_t n) {
  omemoJsRandom((int)p, (int)n);
  return 0;
}

// TODO: don't need EMSCRIPTEN_KEEPALIVE for these two, we should probably say something like OMEMO_USER instead of OMEMO_EXPORT for these
int omemoLoadMessageKey(struct omemoSession *s,
                                     struct omemoMessageKey *k) {
  return omemoJsLoadMessageKey(k->nr, (int)k->dh, (int)k->mk, (int)s);
}

int omemoStoreMessageKey(struct omemoSession *s,
                                      const struct omemoMessageKey *k,
                                      uint64_t n) {
  return omemoJsStoreMessageKey(k->nr, (int)k->dh, (int)k->mk, n, (int)s);
}

EMSCRIPTEN_KEEPALIVE
struct omemoSession *AllocateSession(void) {
  return calloc(1, sizeof(struct omemoSession));
}

EMSCRIPTEN_KEEPALIVE
struct omemoStore *AllocateStore(void) {
  return calloc(1, sizeof(struct omemoStore));
}

