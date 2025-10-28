CFLAGS+=-g -Wall -Wno-pointer-sign -I. -MMD -MP

OMEMOSRCS=c25519.c hacl.c omemo.c
XMPPSRCS=example/xmpp.c example/yxml.c
IMSRCS=example/im.c

ALLBINS=o/test-xmpp \
		o/test-omemo \
		o/im \
		o/generate

ifdef MBED_VENDOR
MBED_FLAGS=$(MBED_VENDOR)/library/libmbedtls.a    \
           $(MBED_VENDOR)/library/libmbedcrypto.a \
           $(MBED_VENDOR)/library/libmbedx509.a   \
        -I $(MBED_VENDOR)/include
else
MBED_FLAGS=-lmbedtls -lmbedcrypto -lmbedx509
endif

DEPS=$(ALLBINS:%=%.d)

all: $(ALLBINS) tags

.PHONY: test
test: test-omemo

$(ALLBINS): | o

o:
	mkdir -p o

o/test-xmpp: test/xmpp.c example/yxml.c example/xmpp.c test/cacert.inc
	$(CC) -o $@ test/xmpp.c example/yxml.c  $(CFLAGS) -Iexample $(MBED_FLAGS)

o/test-omemo: test/omemo.c c25519.c hacl.c omemo.c o/store.inc o/msg.bin
	$(CC) -o $@ test/omemo.c c25519.c hacl.c $(CFLAGS) -DOMEMO_EXPORT=static -static $(MBED_FLAGS)

o/im: $(IMSRCS) $(XMPPSRCS) $(OMEMOSRCS) | o/store.inc test/cacert.inc
	$(CC) -o $@ $^ $(CFLAGS) -Iexample -DIM_NATIVE $(MBED_FLAGS) -lsqlite3

o/generate: test/generate.c $(OMEMOSRCS)
	$(CC) -o $@ $^ $(CFLAGS) $(MBED_FLAGS)

o/msg.bin: test/initsession.py o/bundle.py | test/bot-venv/
	PYTHONPATH=o ./test/bot-venv/bin/python test/initsession.py

test/localhost.crt:
	openssl req -new -x509 -key test/localhost.key -out $@ -days 3650 -config test/localhost.cnf

test/cacert.inc: test/localhost.crt
	(cat test/localhost.crt; printf "\0") | xxd -i -name cacert_pem > $@

o/store.inc o/bundle.py: o/generate
	o/generate o/store.inc o/bundle.py

ESP_DEV?=/dev/ttyUSB0

ifneq (,$(wildcard $(ESP_DEV)))
	ESP_DEVARG= --device=$(ESP_DEV)
endif

ESPIDF_DOCKERCMD=docker run -it --rm -v ${PWD}:/project -u $(shell id -u) -w /project -e HOME=/tmp $(ESP_DEVARG) espressif/idf idf.py -B o/example-esp-im -C example/esp-im

.PHONY: esp-im
esp-im: | o
	$(ESPIDF_DOCKERCMD) build

.PHONY: size-esp-im
size-esp-im: | o
	$(ESPIDF_DOCKERCMD) size-files

.PHONY: esp-upload
esp-upload:
	$(ESPIDF_DOCKERCMD) flash

.PHONY: esp-console
esp-console:
	rlwrap -- socat - $(ESP_DEV),b115200,cfmakeraw,ignoreeof

.PHONY: esp-monitor
esp-monitor:
	$(ESPIDF_DOCKERCMD) monitor

.PHONY: test-xmpp
test-xmpp: o/test-xmpp
	./o/test-xmpp

.PHONY: test-omemo
test-omemo: o/test-omemo
	./o/test-omemo

define IM_INPUT
/login admin@localhost
adminpass
endef
export IM_INPUT

.PHONY: runim
runim: o/im
	rlwrap -P "$$IM_INPUT" ./o/im

.PHONY: start-prosody
start-prosody: test/localhost.crt
	docker-compose -f test/docker-compose.yml up -d --build

.PHONY: stop-prosody
stop-prosody:
	docker-compose -f test/docker-compose.yml down

.PHONY: reset-accounts
reset-accounts:
	# using docker instead of docker-compose is way faster
	docker exec test_prosody_1 sh -c\
	  'prosodyctl deluser admin@localhost && \
	   prosodyctl deluser user@localhost && \
	   prosodyctl register admin localhost adminpass && \
	   prosodyctl register user  localhost userpass'


test/bot-venv/:
	python -m venv test/bot-venv/
	./test/bot-venv/bin/pip install slixmpp==1.8.5
	./test/bot-venv/bin/pip install slixmpp-omemo==1.0.0

start-omemo-bot: | test/bot-venv/
	./test/bot-venv/bin/python test/bot-omemo.py

.PHONY: tags
tags:
	@ctags-exuberant --c-kinds=+p -R --exclude=o --exclude=test/bot-venv

.PHONY: clean
clean:
	rm -rf o

-include $(DEPS)
