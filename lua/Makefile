all: lomemo.so native.so

lomemo.so: lomemo.c ../omemo.c ../c25519.c
	cc -shared -o $@ $^ -fPIC -I .. -I /usr/include/lua5.4 -llua5.4 -lmbedcrypto

native.so: native.c
	cc -shared -o $@ $^ -fPIC -I /usr/include/lua5.4 -llua5.4 -lmbedcrypto -lmbedtls -lmbedx509

.PHONY: clean
clean:
	rm -f native.so lomemo.so
