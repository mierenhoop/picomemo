#!/bin/sh
set -ex
make DRIVERS="hacl.c   mbedtls.c" clean test-omemo test-omemo2
make DRIVERS="c25519.c mbedtls.c" clean test-omemo test-omemo2
make DRIVERS="hacl.c   openssl.c" clean test-omemo test-omemo2
make DRIVERS="c25519.c openssl.c" clean test-omemo test-omemo2
