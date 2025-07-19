#include <sys/random.h>

int omemoRandom(void *d, size_t n) {
  assert(getrandom(d, n, 0) == n);
  return 0;
}

int omemoLoadMessageKey(struct omemoSession *, struct omemoMessageKey *sk) { return 1; }

int omemoStoreMessageKey(struct omemoSession *, const struct omemoMessageKey *sk, uint64_t) { return OMEMO_EUSER;  }
