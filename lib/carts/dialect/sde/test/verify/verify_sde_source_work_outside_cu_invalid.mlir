// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

// FAIL: host scf.for compute at function scope, outside any CU, in a function
// that also contains SDE structure (the sde.mu_alloc). All source executable
// work belongs in a CU.
module {
  func.func @source_outside_cu(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %B = sde.mu_alloc : memref<4xf32>
    scf.for %j = %c0 to %c8 step %c1 {
      %v = memref.load %A[%j] : memref<8xf32>
      memref.store %v, %A[%j] : memref<8xf32>
    }
    return
  }
}

// CHECK: source executable work outside any CU
