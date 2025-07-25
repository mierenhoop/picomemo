/**
 * Copyright 2024 mierenhoop
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>

#include "omemo.h"

#include "test/defaultcallbacks.inc"

FILE *f;

void PrintHex(const uint8_t *p, size_t n) {
  fprintf(f, "bytes.fromhex('");
  for (int i = 0; i < n; i++) fprintf(f, "%02x", p[i]);
  fprintf(f, "')\n");
}

void PrintSer(omemoKey k) {
  omemoSerializedKey ser;
  omemoSerializeKey(ser, k);
  PrintHex(ser, sizeof(ser));
}


int main(int argc, char **argv) {
  assert(argc == 3);
  struct omemoStore store;
  assert(!omemoSetupStore(&store));
  size_t n = omemoGetSerializedStoreSize(&store);
  uint8_t *buf = malloc(n);
  assert(buf);
  omemoSerializeStore(buf, &store);
  f = fopen(argv[1], "w");
  assert(f);
  fprintf(f, "unsigned char store_inc[] = {");
  for (int i = 0; i < n; i++) {
    if (i % 12 == 0)
      fprintf(f, "\n ");
    fprintf(f, " 0x%02x,", buf[i]);
  }
  fprintf(f, "\n};\nunsigned int store_inc_len = %ld;\n", n);
  fclose(f);

  f=fopen(argv[2], "w");
  assert(f);
  fprintf(f, "ik=");PrintSer(store.identity.pub);
  fprintf(f, "spk=");PrintSer(store.cursignedprekey.kp.pub);
  fprintf(f, "spks=");PrintHex(store.cursignedprekey.sig, 64);
  fprintf(f, "spk_id=%d\n",store.cursignedprekey.id);
  fprintf(f, "pks={}\n");
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++) {
    fprintf(f, "pks[%d]=",store.prekeys[i].id);
    PrintSer(store.prekeys[i].kp.pub);
  }
  fclose(f);
}
