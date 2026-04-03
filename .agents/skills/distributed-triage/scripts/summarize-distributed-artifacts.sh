#!/usr/bin/env bash
set -euo pipefail

root=${1:-.}

echo "[logs]" >&2
find "$root" \( -name 'arts.log' -o -name 'omp.log' \) -print | sort
echo >&2

echo "[counters]" >&2
find "$root" \( -name 'cluster.json' -o -name 'n*.json' \) -print | sort
echo >&2

echo "[configs]" >&2
find "$root" \( -name 'arts.cfg' -o -name '.arts_cflags' -o -name '.omp_cflags' \) -print | sort
