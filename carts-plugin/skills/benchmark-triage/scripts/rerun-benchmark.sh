#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <suite/name> [size=small] [threads=2] [stage_csv]" >&2
  exit 2
fi

bench=$1
size=${2:-small}
threads=${3:-2}
stages=${4:-}

if [[ -n "$stages" ]]; then
  dekk carts triage-benchmark "$bench" --size "$size" --threads "$threads" --stages "$stages"
else
  dekk carts triage-benchmark "$bench" --size "$size" --threads "$threads"
fi
