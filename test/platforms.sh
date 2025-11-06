#!/bin/bash

set -ex

make clean mbedtls o/test-omemo && rm o/test-omemo
CC=aarch64-linux-gnu-gcc AR=aarch64-linux-gnu-gcc-ar \
  make -j -C mbedtls lib
CC=aarch64-linux-gnu-gcc MBED_VENDOR=mbedtls \
  make o/test-omemo
qemu-aarch64 o/test-omemo

make clean mbedtls o/test-omemo && rm o/test-omemo
emmake make -j -C mbedtls lib
CFLAGS="-Os -flto -s SINGLE_FILE=1" MBED_VENDOR=mbedtls \
  emmake make o/test-omemo
node --experimental-global-webcrypto o/test-omemo
