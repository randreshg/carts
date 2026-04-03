#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'AM->invalidate()' \
  -e 'invalidateAndRebuildGraphs' \
  -e 'invalidateFunction' \
  -e 'dbAnalysis.invalidate' \
  -e 'edtAnalysis.invalidate' \
  -e '->invalidate()' \
  -- include lib docs
