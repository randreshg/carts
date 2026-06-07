#!/usr/bin/env bash
set -euo pipefail

root=${1:-external/carts-benchmarks/results}

find "$root" \
  \( -name 'arts.log' -o -name 'omp.log' -o -name '.arts_cflags' -o -name '.omp_cflags' -o -name 'arts.cfg' \) \
  -print | sort
