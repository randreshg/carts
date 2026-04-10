#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'LoweringContractInfo' \
  -e 'PatternSemantics' \
  -e 'PatternContract' \
  -e 'distribution_' \
  -e 'PartitionStrategy' \
  -e 'AcquireRewriteContract' \
  -- include lib docs .agents
