#!/usr/bin/env bash
set -euo pipefail

if [[ ${1:-} == "--json" ]]; then
  carts pipeline --json
else
  carts pipeline
fi
