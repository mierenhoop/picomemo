#!/bin/sh
set -ex
V="valgrind --tool=memcheck --track-origins=yes --error-exitcode=1"
TESTRUNTOOL="$V" DRIVERS="hacl.c   mbedtls.c" make clean test-omemo test-omemo2
TESTRUNTOOL="$V" DRIVERS="c25519.c mbedtls.c" make clean test-omemo test-omemo2
TESTRUNTOOL="$V" DRIVERS="hacl.c   openssl.c" make clean test-omemo test-omemo2
TESTRUNTOOL="$V" DRIVERS="c25519.c openssl.c" make clean test-omemo test-omemo2
