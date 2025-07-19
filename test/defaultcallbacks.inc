#include <sys/random.h>

int omemoRandom(void *d, size_t n) { return getrandom(d, n, 0) != n; }

int omemoLoadMessageKey(struct omemoSession *, struct omemoMessageKey *sk) { return OMEMO_EUSER; }

int omemoStoreMessageKey(struct omemoSession *, const struct omemoMessageKey *sk, uint64_t) { return OMEMO_EUSER;  }
