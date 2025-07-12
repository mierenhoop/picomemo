LUA=lua5.4
CFLAGS= -Wall -Wextra -Wno-unused -Wno-pointer-sign -Wno-sign-compare

STORES=../o/store1.bin ../o/store2.bin

all: lomemo.so native.so test

test: lomemo.so native.so $(STORES)
	busted --lua=$(LUA)

$(STORES): lomemo.so
	for store in $(STORES); do \
		mkdir -p $$(dirname $$store); \
		lua5.4 -e 'io.write(require"lomemo".SetupStore():Serialize())' > $$store; \
	done

lomemo.so: lomemo.c ../omemo.c ../c25519.c
	cc -shared -o $@ $^ $(CFLAGS) -fPIC -I .. -I /usr/include/$(LUA) -l$(LUA) -lmbedcrypto

native.so: native.c
	cc -shared -o $@ $^ $(CFLAGS) -fPIC -I /usr/include/$(LUA) -l$(LUA) -lmbedcrypto -lmbedtls -lmbedx509

.PHONY: clean
clean:
	rm -f native.so lomemo.so
