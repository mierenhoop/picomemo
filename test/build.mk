ALLBINS+=o/test-xmpp o/test-omemo o/generate

o/test-xmpp: test/xmpp.c example/yxml.c example/xmpp.c test/cacert.inc
	$(CC) -o $@ test/xmpp.c example/yxml.c  $(CFLAGS) -Iexample $(MBED_FLAGS)

o/test-omemo: test/omemo.c c25519.c hacl.c omemo.c o/store.inc o/msg.bin
	$(CC) -o $@ test/omemo.c c25519.c hacl.c $(CFLAGS) -DOMEMO_EXPORT=static -static $(MBED_FLAGS)

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

.PHONY: test-xmpp
test-xmpp: o/test-xmpp
	./o/test-xmpp

.PHONY: test-omemo
test-omemo: o/test-omemo
	./o/test-omemo

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

