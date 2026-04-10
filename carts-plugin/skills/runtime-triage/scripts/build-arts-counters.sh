#!/usr/bin/env bash
set -euo pipefail

level=${1:-2}
shift $(( $# > 0 ? 1 : 0 ))

dekk dekk carts build --arts --counters "$level" "$@"
