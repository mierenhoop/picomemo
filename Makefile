### CONFIG ###
#
# make DRIVERS="hacl.c mbedtls.c" MBED_VENDOR=mbedtls
#
# For DRIVERS choose one of hacl.c OR c25519.c,
#             AND one of mbedtls.c OR openssl.c
#
# MBED_VENDOR=mbedtls will compile with an "in-tree" mbedtls,
# useful when you don't have mbedtls installed.
# - Before that you have to $ make mbedtls && make -C mbedtls lib
#

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

CFLAGS+=-Wall -Wno-pointer-sign -Wno-unused-function -I. -MMD -MP

GENERATED:=omemo0.c omemo0.h omemo2.c omemo2.h
OMEMOSRCS:=c25519.c hacl.c omemo.c

all: $(GENERATED) o/picomemo0.so o/picomemo2.so tags

o:
	mkdir -p o

SO_BUILD=$(CC) -shared -o $@ $^ $(CFLAGS) $(OMEMOCFLAGS) -fvisibility=hidden $(LDFLAGS) $(LIBS)
EXPORTDEF:="__attribute__((visibility(\"default\")))"

o/picomemo0.so: omemo0.c $(DRIVERS) | o
	$(SO_BUILD) -DOMEMO0_EXPORT=$(EXPORTDEF)

o/picomemo2.so: omemo2.c $(DRIVERS) | o
	$(SO_BUILD) -DOMEMO2_EXPORT=$(EXPORTDEF)

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

# These can be omitted
include example/build.mk
include test/build.mk

-include $(wildcard o/*.d)
