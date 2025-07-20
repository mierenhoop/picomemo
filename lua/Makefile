LUA?=lua5.4
CFLAGS= -Wall -Wextra -Wno-unused -Wno-pointer-sign -Wno-sign-compare

STORES=../o/storea1.bin ../o/storeb1.bin

all: lomemo.so lomemo2.so test

test: lomemo.so $(STORES)
	busted --lua=$(LUA)

$(STORES): lomemo.so
	for store in $(STORES); do \
		mkdir -p $$(dirname $$store); \
		$(LUA) -e 'io.write(require"lomemo".SetupStore():Serialize())' > $$store; \
	done

lomemo.so: lomemo.c ../omemo.c ../c25519.c
	cc -shared -o $@ $^ $(CFLAGS) -fPIC -I .. -I /usr/include/$(LUA) -l$(LUA) -lmbedcrypto

lomemo2.so: lomemo.c ../omemo.c ../c25519.c
	cc -shared -o $@ $^ $(CFLAGS) -DOMEMO2 -fPIC -I .. -I /usr/include/$(LUA) -l$(LUA) -lmbedcrypto

.PHONY: clean
clean:
	rm -f lomemo.so lomemo2.so $(STORES)
