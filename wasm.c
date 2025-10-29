#include <emscripten.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include "omemo.h"

// rm -f o/test-omemo* && CFLAGS="-Os -s -flto -DOMEMO2 -s EXPORTED_FUNCTIONS=_malloc,_main" MBED_VENDOR=mbedtls emmake make o/test-omemo && ls -lah o/test-omemo.wasm
//
// emcc -o /tmp/omemo.wasm omemo.c wasm.c hacl.c -I mbedtls/include mbedtls/library/libmbedcrypto.a -Os -s -flto --no-entry -sEXPORTED_FUNCTIONS=_malloc,_free,_AllocateStore,_AllocateSession,_omemoSerializeKey,_omemoSetupStore,_omemoRefillPreKeys,_omemoRotateSignedPreKey,_omemoGetSerializedStoreSize,_omemoSerializeSession,_omemoDeserializeSession,_omemoInitiateSession,_omemoEncryptKey,_omemoDecryptKey,_omemoEncryptMessage,_omemoDecryptMessage --js-library omemo.js
// TODO: implement En/DecryptMessage in js

extern int omemoJsLoadMessageKey(int,int,int,int);
extern int omemoJsStoreMessageKey(int,int,int,int,int);

int omemoRandom(void *p, size_t n) {
  EM_ASM({
    let buf = new Uint8Array($1);
    crypto.getRandomValues(buf);
    for (let i = 0; i < $1; i++)
        Module.HEAPU8[$0 + i] = buf[i];
  }, (int)p, n);
  return 0;
}

int omemoLoadMessageKey(struct omemoSession *s,
                                     struct omemoMessageKey *k) {
  return omemoJsLoadMessageKey(k->nr, (int)k->dh, (int)k->mk, (int)s);
}

int omemoStoreMessageKey(struct omemoSession *s,
                                      const struct omemoMessageKey *k,
                                      uint64_t n) {
  return omemoJsStoreMessageKey(k->nr, (int)k->dh, (int)k->mk, n, (int)s);
}

struct omemoSession *AllocateSession(void) {
  return calloc(1, sizeof(struct omemoSession));
}

struct omemoStore *AllocateStore(void) {
  return calloc(1, sizeof(struct omemoStore));
}

