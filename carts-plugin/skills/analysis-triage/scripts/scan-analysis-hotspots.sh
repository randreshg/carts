#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'LoweringFactUtils' \
  -e 'DbUtils' \
  -e 'EdtUtils' \
  -e 'VerifySdeLowered' \
  -e 'VerifyArtsObjectsOnly' \
  -e 'phase-ordering' \
  -- include lib docs .agents
