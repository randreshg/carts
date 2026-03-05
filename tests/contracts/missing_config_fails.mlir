// RUN: not %carts-compile %S/inputs/uniform_block.mlir --O3 --arts-config %t.missing.cfg 2>&1 | %FileCheck %s

// CHECK: No arts.cfg file found at
// CHECK-NEXT: Invalid ARTS configuration. Provide a valid --arts-config path or place a valid arts.cfg in the working directory.
