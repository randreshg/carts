#!/usr/bin/env bash
set -euo pipefail

root=${1:-.}

find "$root" \
  \( -name 'arts.log' -o -name 'omp.log' -o -name 'cluster.json' -o -name 'n*.json' \) \
  -print | sort
