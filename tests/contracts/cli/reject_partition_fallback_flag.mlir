// RUN: not %carts-compile %S/../inputs/uniform_block.mlir --partition-fallback=coarse 2>&1 | %FileCheck %s

// CHECK: Unknown command line argument '--partition-fallback=coarse'
