ALLBINS+=o/im

XMPPSRCS:=example/xmpp.c example/yxml.c
IMSRCS:=example/im.c

o/im: $(IMSRCS) $(XMPPSRCS) $(OMEMOSRCS) | o/store.inc test/cacert.inc
	$(CC) -o $@ $^ $(CFLAGS) -Iexample -DIM_NATIVE $(MBED_FLAGS) -lsqlite3

define IM_INPUT
/login admin@localhost
adminpass
endef
export IM_INPUT

.PHONY: runim
runim: o/im
	rlwrap -P "$$IM_INPUT" ./o/im

### ESP32 ###

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
