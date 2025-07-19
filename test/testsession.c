#include <stdio.h>
#include <assert.h>

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
  printf("%d\n", omemoDecryptKey(&session, &st, payload, true, buf, n));

  for (int i = 0;i < sizeof(payload);i++) printf("%0x", payload[i]);
}
