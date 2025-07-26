#!/bin/sh

CFLAGS="                       " make clean test-omemo
CFLAGS="-DOMEMO2               " make clean test-omemo
CFLAGS="         -DOMEMO_NOHACL" make clean test-omemo
CFLAGS="-DOMEMO2 -DOMEMO_NOHACL" make clean test-omemo
