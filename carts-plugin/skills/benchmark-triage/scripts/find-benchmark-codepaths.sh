#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'triage-benchmark' \
  -e 'carts-benchmarks' \
  -e 'SdeStorageToArtsDb' \
  -e 'SdeAccessesToArtsDeps' \
  -e 'FinalizeSdeToArts' \
  -e 'DistributionPlanning' \
  -e 'ReductionStrategy' \
  -e 'DbTransforms' \
  -e 'DistributionHeuristics' \
  -e 'external/carts-benchmarks/common/carts.mk' \
  -- tools lib docs external .agents
