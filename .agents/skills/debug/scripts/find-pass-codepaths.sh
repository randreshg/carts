#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <token>" >&2
  exit 2
fi

token=$1
cd "$(git rev-parse --show-toplevel)"

git grep -n "$token" -- include lib docs tools .agents
