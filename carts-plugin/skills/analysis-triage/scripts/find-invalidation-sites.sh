#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'replaceAllUses' \
  -e 'erase()' \
  -e 'removeAttr' \
  -e 'setAttr' \
  -e 'VerifySdeLowered' \
  -e 'VerifyArtsObjectsOnly' \
  -- include lib docs
