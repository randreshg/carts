// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @root_without_layout(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<8xf32> array_id(0)
      sde.cu_region <single> {
        sde.yield
      }
      sde.yield
    }
    return
  }
}

// CHECK: commits array root provenance but the enclosing sde.su_iterate has no arrayLayout
