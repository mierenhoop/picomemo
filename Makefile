### CONFIG ###
#
# Example configurations:
# $ DRIVERS="hacl.c openssl.c" CFLAGS="-O3" LDFLAGS="-flto" make
# $ DRIVERS="hacl.c mbedtls.c" MBED_VENDOR=mbedtls          make
#
# For DRIVERS choose one of hacl.c OR c25519.c,
#             AND one of mbedtls.c OR openssl.c
#
# MBED_VENDOR=mbedtls will compile with an "in-tree" mbedtls,
# useful when you don't have mbedtls installed.
# - Before that you have to $ make mbedtls && make -C mbedtls lib
#
###

V:=1.2.0
SO_VERSION:=1

ifndef DRIVERS
DRIVERS:=hacl.c mbedtls.c
endif

ifneq ($(filter mbedtls.c,$(DRIVERS)),)
ifdef MBED_VENDOR
LIBS+=$(MBED_VENDOR)/library/libmbedcrypto.a
OMEMOCFLAGS:=-I $(MBED_VENDOR)/include
else
LIBS+=-lmbedcrypto
endif
endif

ifneq ($(filter openssl.c,$(DRIVERS)),)
LIBS+=-lssl -lcrypto
endif

CFLAGS?=-O2 -g
CFLAGS+=-Wall -Wno-pointer-sign -Wno-unused-function -I. -MMD -MP

GENERATED:=gen/omemo0.c \
           gen/omemo0.h \
           gen/omemo2.c \
           gen/omemo2.h

OMEMOSRCS:=c25519.c hacl.c omemo.c

.PHONY: all
all: $(GENERATED) lib tags

.PHONY: lib
lib: o/libpicomemo.so.$V

o:
	mkdir -p o

SO_BUILD=$(CC) -shared -o $@ $^ $(CFLAGS) $(OMEMOCFLAGS) $(LDFLAGS) \
		 $(LIBS) -fPIC -fvisibility=hidden

EXPORTDEF:="__attribute__((visibility(\"default\")))"

o/libpicomemo.so.$V: gen/omemo0.c gen/omemo2.c $(DRIVERS) | o
	$(SO_BUILD) -DOMEMO0_EXPORT=$(EXPORTDEF) -DOMEMO2_EXPORT=$(EXPORTDEF) \
		-Wl,-soname,libpicomemo.so.$(SO_VERSION)

$(GENERATED) &: omemo.c omemo.h gen/split.lua
	lua gen/split.lua

### MBEDTLS VENDORING ###

MBEDTLSCKSUM:=ec35b18a6c593cf98c3e30db8b98ff93e8940a8c4e690e66b41dfc011d678110
.DELETE_ON_ERROR: mbedtls.tar.bz2
mbedtls.tar.bz2:
	curl -Lo $@ "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.4/mbedtls-3.6.4.tar.bz2"
	echo "$(MBEDTLSCKSUM) mbedtls.tar.bz2" | sha256sum -c

mbedtls: mbedtls.tar.bz2
	rm -rf $@
	tar -xjf $< --strip-components=1 --one-top-level=$@

###

.PHONY: tags
tags:
	@ctags-exuberant --c-kinds=+p -R --exclude=o --exclude=test/bot-venv

.PHONY: clean
clean:
	rm -rf o mbedtls

# These can be omitted
include example/build.mk
include test/build.mk

-include $(wildcard o/*.d)
