// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @unsupported_direct_cu_carrier() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%i: index):
      sde.cu_task {
      }
      sde.yield
    }
    return
  }
}

// CHECK: 'sde.cu_task' op is directly inside an sde.su_iterate body
// CHECK: may contain only direct-boundary CUs
