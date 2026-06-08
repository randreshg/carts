#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n \
  -e 'DbOwnerMapRealization' \
  -e 'DbDistributedEligibility' \
  -e 'DistributionPlanning' \
  -e 'ConvertSdeToCodir' \
  -e 'ConvertCodirToArts' \
  -e 'distributed_db_init' \
  -e 'distributed_db_init_worker' \
  -e 'artsGetTotalNodes' \
  -e 'artsGuidGetRank' \
  -e 'route = linearIndex' \
  -- docs include lib tools .agents
