### CONFIG ###

DRIVERSRCS:=hacl.c
DRIVERLIBS:=

ifndef CFLAGS
CFLAGS+=-g
endif
CFLAGS+=-Wall -Wno-pointer-sign -Wno-unused-function -I. -MMD -MP

###

GENERATED:=omemo0.c omemo0.h omemo2.c omemo2.h
OMEMOSRCS:=c25519.c hacl.c omemo.c

ALLBINS:=o/picomemo0.so o/picomemo2.so

ifdef MBED_VENDOR
MBED_FLAGS:=$(MBED_VENDOR)/library/libmbedtls.a    \
            $(MBED_VENDOR)/library/libmbedcrypto.a \
            $(MBED_VENDOR)/library/libmbedx509.a   \
        -I  $(MBED_VENDOR)/include
else
MBED_FLAGS:=-lmbedtls -lmbedcrypto -lmbedx509
endif

DEPS:=$(ALLBINS:%=%.d)

all: $(ALLBINS) $(GENERATED) test-omemo tags

$(ALLBINS): | o

o:
	mkdir -p o

o/picomemo0.so: omemo0.c $(DRIVERSRCS) | o
	$(CC) -shared -o $@ $^ $(CFLAGS) $(LDFLAGS) -lmbedcrypto

o/picomemo2.so: omemo2.c $(DRIVERSRCS) | o
	$(CC) -shared -o $@ $^ $(CFLAGS) $(LDFLAGS) -lmbedcrypto

$(GENERATED): omemo.c omemo.h split.lua
	lua split.lua

### MBEDTLS VENDORING ###

MBEDTLSCKSUM:=ec35b18a6c593cf98c3e30db8b98ff93e8940a8c4e690e66b41dfc011d678110
.DELETE_ON_ERROR: mbedtls.tar.bz2
mbedtls.tar.bz2:
	curl -Lo $@ "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.4/mbedtls-3.6.4.tar.bz2"
	echo "$(MBEDTLSCKSUM) mbedtls.tar.bz2" | sha256sum -c

mbedtls: mbedtls.tar.bz2
	rm -rf $@
	tar -xjf $< --strip-components=1 --one-top-level=$@

.PHONY: tags
tags:
	@ctags-exuberant --c-kinds=+p -R --exclude=o --exclude=test/bot-venv

.PHONY: clean
clean:
	rm -rf o mbedtls

# These are optional
include example/build.mk
include test/build.mk

-include $(DEPS)
