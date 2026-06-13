#!/bin/sh
set -ex
V="valgrind --tool=memcheck --track-origins=yes --error-exitcode=1"
TESTRUNTOOL="$V" DRIVERS="hacl   mbedtls" make clean test-omemo test-omemo2
TESTRUNTOOL="$V" DRIVERS="c25519 mbedtls" make clean test-omemo test-omemo2
TESTRUNTOOL="$V" DRIVERS="hacl   openssl" make clean test-omemo test-omemo2
TESTRUNTOOL="$V" DRIVERS="c25519 openssl" make clean test-omemo test-omemo2
