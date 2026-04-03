#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <tests/contracts/name.mlir> <stage>" >&2
  exit 2
fi

path=$1
stage=$2

mkdir -p "$(dirname "$path")"

cat > "$path" <<EOF
// RUN: %carts-compile %s --pipeline ${stage} | %FileCheck %s
// CHECK: TODO

module {
  func.func @todo() {
    return
  }
}
EOF

echo "$path"
