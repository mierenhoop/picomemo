TESTCFLAGS=$(CFLAGS) -g

all: test-omemo test-omemo2

o/test-xmpp: test/xmpp.c example/yxml.c example/xmpp.c test/cacert.inc
	$(CC) -o $@ test/xmpp.c example/yxml.c $(TESTCFLAGS) -Iexample -lmbedtls -lmbedcrypto -lmbedx509

TESTOMEMO_DEPS=test/omemo.c omemo.c $(sort c25519.c hacl.c $(DRIVERS))
TESTOMEMO_BUILD=$(CC) -o $@ $(filter-out omemo.c, $(TESTOMEMO_DEPS)) \
				$(TESTCFLAGS) $(OMEMOCFLAGS) $(LIBS)

o/test-omemo: $(TESTOMEMO_DEPS) o/store.inc o/msg.bin
	$(TESTOMEMO_BUILD)

o/test-omemo2: $(TESTOMEMO_DEPS) o/store2.inc o/msg2.bin
	$(TESTOMEMO_BUILD) -DOMEMO2

o/generate: test/generate.c omemo.c $(DRIVERS) | o
	$(CC) -o $@ $^ $(TESTCFLAGS) $(LIBS)

o/generate2: test/generate.c omemo.c $(DRIVERS) | o
	$(CC) -o $@ $^ $(TESTCFLAGS) $(LIBS) -DOMEMO2

o/msg.bin: test/initsession.py o/bundle.py | test/bot-venv/
	PYTHONPATH=o ./test/bot-venv/bin/python test/initsession.py bundle

o/msg2.bin: test/initsession.py o/bundle2.py | test/bot-venv/
	PYTHONPATH=o ./test/bot-venv/bin/python test/initsession.py bundle2

test/localhost.crt:
	openssl req -new -x509 -key test/localhost.key -out $@ -days 3650 -config test/localhost.cnf

test/cacert.inc: test/localhost.crt
	(cat test/localhost.crt; printf "\0") | xxd -i -name cacert_pem > $@

o/store.inc o/bundle.py: o/generate
	o/generate o/store.inc o/bundle.py

o/store2.inc o/bundle2.py: o/generate2
	o/generate2 o/store2.inc o/bundle2.py

.PHONY: test-xmpp
test-xmpp: o/test-xmpp
	./o/test-xmpp

.PHONY: test-omemo
test-omemo: o/test-omemo
	./o/test-omemo

.PHONY: test-omemo2
test-omemo2: o/test-omemo2
	./o/test-omemo2

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

