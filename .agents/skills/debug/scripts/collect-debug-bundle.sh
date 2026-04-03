#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <input.{c,cpp,mlir}> <outdir> [stage=db-partitioning] [extra carts compile args...]" >&2
  exit 2
fi

input=$1
outdir=$2
stage=${3:-db-partitioning}

if [[ $# -ge 3 ]]; then
  shift 3
else
  shift 2
fi

mkdir -p "$outdir"

carts doctor > "${outdir}/doctor.txt"
carts pipeline --json > "${outdir}/pipeline-manifest.json"
carts compile "$input" --diagnose --diagnose-output "${outdir}/diagnose.json" "$@" >/dev/null
carts compile "$input" --pipeline="$stage" "$@" > "${outdir}/${stage}.mlir"
