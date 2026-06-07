// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

// FAIL: a memref.store (non-loop source compute) sits directly in an SU body.
// All source executable work belongs in a CU, not only scf loops.
module {
  func.func @store_in_su(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %z = arith.constant 0.000000e+00 : f32
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%i: index):
      memref.store %z, %A[%i] : memref<8xf32>
      sde.yield
    }
    return
  }
}

// CHECK: raw scf/source compute directly inside an SU body
