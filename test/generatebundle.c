#include <stdio.h>
#include <assert.h>

#include "omemo.h"

#include "test/defaultcallbacks.inc"
#ifdef OMEMO2
#include "test/store2.inc"
#else
#include "test/store.inc"
#endif

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

int main() {
#ifdef OMEMO2
  f=fopen("test/bundle2.py", "w");
#else
  f=fopen("test/bundle.py", "w");
#endif
  assert(f);
  struct omemoStore st;
  omemoDeserializeStore(store_inc, store_inc_len, &st);
  fprintf(f, "ik=");PrintSer(st.identity.pub);
  fprintf(f, "spk=");PrintSer(st.cursignedprekey.kp.pub);
  fprintf(f, "spks=");PrintHex(st.cursignedprekey.sig, 64);
  fprintf(f, "spk_id=%d\n",st.cursignedprekey.id);
  fprintf(f, "pks={}\n");
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++) {
    fprintf(f, "pks[%d]=",st.prekeys[i].id);
    PrintSer(st.prekeys[i].kp.pub);
  }
  fclose(f);
}
