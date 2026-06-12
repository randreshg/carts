#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'distribution_' \
  -e 'PartitionStrategy' \
  -e 'LoweringFact' \
  -- include lib docs .agents
