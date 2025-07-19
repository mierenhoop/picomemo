CFLAGS=-g -std=c99 -Wall -Wno-pointer-sign -I. -MMD -MP

OMEMOSRCS=c25519.c omemo.c
XMPPSRCS=example/xmpp.c example/yxml.c
IMSRCS=example/im.c

ALLBINS=o/test-xmpp \
		o/test-omemo \
		o/test-omemo2 \
		o/im \
		o/generatestore \
		o/generatebundle

DEPS=$(ALLBINS:%=%.d)

all: $(ALLBINS) tags

.PHONY: test
test: test-omemo

$(ALLBINS): | o

o:
	mkdir -p o

o/test-xmpp: test/xmpp.c example/yxml.c | example/xmpp.c test/cacert.inc
	$(CC) -o $@ $^ $(CFLAGS) -Iexample -lmbedcrypto -lmbedtls -lmbedx509

o/test-omemo: test/omemo.c c25519.c | omemo.c o/msg.bin
	$(CC) -o $@ $^ $(CFLAGS) -lmbedcrypto

o/test-omemo2: test/omemo.c c25519.c | omemo.c
	$(CC) -o $@ $^ $(CFLAGS) -DOMEMO2 -lmbedcrypto

o/im: $(IMSRCS) $(XMPPSRCS) $(OMEMOSRCS) | test/store.inc test/cacert.inc
	$(CC) -o $@ $^ $(CFLAGS) -Iexample -DIM_NATIVE -lmbedcrypto -lmbedtls -lmbedx509 -lsqlite3

o/generatestore: test/generatestore.c $(OMEMOSRCS) | test/defaultcallbacks.inc
	$(CC) -o $@ $^ $(CFLAGS) -lmbedcrypto

o/generatebundle: test/generatebundle.c $(OMEMOSRCS) | test/defaultcallbacks.inc test/store.inc
	$(CC) -o $@ $^ $(CFLAGS) -lmbedcrypto

test/bundle.py: o/generatebundle
	./o/generatebundle

o/msg.bin: test/initsession.py test/bundle.py | test/bot-venv/
	./test/bot-venv/bin/python test/initsession.py

test/localhost.crt:
	openssl req -new -x509 -key test/localhost.key -out $@ -days 3650 -config test/localhost.cnf

test/cacert.inc: test/localhost.crt
	(cat test/localhost.crt; printf "\0") | xxd -i -name cacert_pem > $@

test/store.inc: o/generatestore
	o/generatestore | xxd -i -name store_inc > $@

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

.PHONY: test
test-xmpp: o/test-xmpp
	./o/test-xmpp

.PHONY: test-omemo
test-omemo: o/test-omemo
	./o/test-omemo

.PHONY: test-omemo2
test-omemo2: o/test-omemo2
	./o/test-omemo2

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
