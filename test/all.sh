#!/bin/sh

# TODO: move to DRIVERS="..."
CFLAGS="                       " make clean test-omemo test-omemo2
CFLAGS="         -DOMEMO_NOHACL" make clean test-omemo test-omemo2
