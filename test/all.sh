#!/bin/sh
set -ex
make DRIVERS="hacl   mbedtls" clean test-omemo test-omemo2
make DRIVERS="c25519 mbedtls" clean test-omemo test-omemo2
make DRIVERS="hacl   openssl" clean test-omemo test-omemo2
make DRIVERS="c25519 openssl" clean test-omemo test-omemo2
