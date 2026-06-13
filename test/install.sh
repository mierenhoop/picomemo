#!/bin/sh
apt-get update
apt-get install -y gcc libmbedtls-dev make lua5.4 \
  curl bzip2 \
  python-is-python3 python3-jinja2 python3-jsonschema \
  gcc-aarch64-linux-gnu qemu-user \
  valgrind emscripten nodejs
