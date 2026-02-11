// RUN: not %carts-run %S/../examples/addition/addition.mlir --O3 --arts-config %t.missing.cfg 2>&1 | %FileCheck %s

// CHECK: Error: ARTS configuration file is required.
// CHECK-NEXT: Provide --arts-config <path> or place arts.cfg in the working directory.

