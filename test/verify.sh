#!/bin/bash
set -ex
# Verify that split.lua's generated artifacts are up to date at HEAD
lua split.lua
[[ -n "$(git status --porcelain)" ]] && exit 1
exit 0
