#!/bin/bash
set -ex
# Verify that split.lua's generated artifacts are up to date at HEAD
lua gen/split.lua
make amalg-c25519 amalg-hacl
[[ -n "$(git status --porcelain)" ]] && exit 1
exit 0
