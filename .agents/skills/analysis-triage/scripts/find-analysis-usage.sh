#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <pass-or-token>" >&2
  exit 2
fi

query=$1
cd "$(git rev-parse --show-toplevel)"

git grep -n "$query" -- include lib docs .agents
echo
git grep -n \
  -e 'AnalysisManager' \
  -e 'getDbAnalysis' \
  -e 'getEdtAnalysis' \
  -e 'getLoopAnalysis' \
  -e 'AnalysisDependencies' \
  -- include lib docs .agents | grep -F "$query" || true
