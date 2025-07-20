CFLAGS=-O3

all: hacl c25519

SRCS=benchmark.c ./Hacl_Ed25519.c ./Hacl_Curve25519_51.c ./Hacl_Hash_SHA2.c ../c25519.c

hacl: $(SRCS)
	$(CC) -o $@ $^ $(CFLAGS) -DHACL -I.

c25519: $(SRCS)
	$(CC) -o $@ $^ $(CFLAGS) -I.

.PHONY: clean
clean:
	rm -f hacl c25519
