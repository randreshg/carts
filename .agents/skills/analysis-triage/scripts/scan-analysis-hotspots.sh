#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'AnalysisDependencies' \
  -e 'AM->invalidate' \
  -e 'invalidateAndRebuildGraphs' \
  -e 'invalidateFunction' \
  -e 'getDbAnalysis' \
  -e 'getEdtAnalysis' \
  -e 'getLoopAnalysis' \
  -e 'phase-ordering' \
  -- include lib docs .agents
