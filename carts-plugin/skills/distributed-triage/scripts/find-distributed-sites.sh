#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'DbOwnerMapRealization' \
  -e 'DbDistributedEligibility' \
  -e 'DistributionPlanning' \
  -e 'SdeStorageToArtsDb' \
  -e 'SdeAccessesToArtsDeps' \
  -e 'FinalizeSdeToArts' \
  -e 'distributed_db_init' \
  -e 'distributed_db_init_worker' \
  -e 'artsGetTotalNodes' \
  -e 'artsGuidGetRank' \
  -e 'route = linearIndex' \
  -- docs include lib tools .agents
