#!/usr/bin/env bash
set -euo pipefail

if [[ ${1:-} == "--json" ]]; then
  dekk carts pipeline --json
else
  dekk carts pipeline
fi
