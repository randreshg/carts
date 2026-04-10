#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <pass-stem>" >&2
  exit 2
fi

stem=$1
cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e "${stem}" \
  -e "k${stem}_reads" \
  -e "k${stem}_invalidates" \
  -e 'AnalysisDependencyInfo' \
  -- include lib docs
