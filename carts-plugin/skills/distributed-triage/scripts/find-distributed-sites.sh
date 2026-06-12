#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'DbDistributedOwnershipRealization' \
  -e 'DbDistributedEligibility' \
  -e 'DistributionPlanning' \
  -e 'SdeStorageToArtsDb' \
  -e 'SdeAccessesToArtsDeps' \
  -e 'FinalizeSdeToArts' \
  -e 'DbDistributedRuntimeInit' \
  -e 'distributed_db_init' \
  -e 'distributed_db_init_worker' \
  -e 'arts_rt.db_guid_reserve' \
  -e 'arts_rt.db_create_with_guid_local' \
  -e 'artsGetTotalNodes' \
  -e 'artsGuidGetRank' \
  -- docs include lib tools .agents
