#!/bin/bash

set -ex

# This is a hack to generate the stores with host compiler, TODO: have HOSTCC
make clean mbedtls o/test-omemo{,2} && rm -f o/{test-omemo{,2},mbedtls.o,hacl.o}
CC=aarch64-linux-gnu-gcc AR=aarch64-linux-gnu-gcc-ar \
  make -j -C mbedtls lib
CFLAGS="-static" CC=aarch64-linux-gnu-gcc MBED_VENDOR=mbedtls \
  make o/test-omemo{,2} -o o/generate -o o/generate2
qemu-aarch64 o/test-omemo
qemu-aarch64 o/test-omemo2

make clean mbedtls o/test-omemo{,2} && rm -f o/{test-omemo{,2},mbedtls.o,hacl.o}
emmake make -j -C mbedtls lib
CFLAGS="-Os -flto -s SINGLE_FILE=1" MBED_VENDOR=mbedtls \
  emmake make o/test-omemo{,2} -o o/generate -o o/generate2
node --experimental-global-webcrypto o/test-omemo
node --experimental-global-webcrypto o/test-omemo2
