#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "omemo.h"

#include "test/defaultcallbacks.inc"
#include "test/store.inc"

struct omemoSession session;
struct omemoStore st;
int main() {
  // TODO: give store.inc other name so we can integrate this file in test/omemo.c
  uint8_t buf[1000];
  omemoDeserializeStore(store, store_len, &st);
  FILE *f = fopen("o/msg.bin", "r");
  int n = fread(buf, 1, 1000, f);
  assert(n > 0);
  fclose(f);

  omemoKeyPayload payload;
  assert(!omemoDecryptKey(&session, &st, payload, true, buf, n));

  uint8_t exp[32];
  memset(exp,    0x55, 16);
  memset(exp+16, 0xaa, 16);
  assert(!memcmp(exp, payload, 32));
}
