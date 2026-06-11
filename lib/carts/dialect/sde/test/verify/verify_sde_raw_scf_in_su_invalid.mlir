// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

// FAIL: raw scf.for directly inside an SU body. SU bodies compose executable
// leaf CUs; compute-local scf must live inside a direct child CU.
module {
  func.func @raw_scf_in_su(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c8 step %c1 {
        %v = memref.load %A[%j] : memref<8xf32>
        memref.store %v, %A[%j] : memref<8xf32>
      }
      sde.yield
    }
    return
  }
}

// CHECK: 'scf.for' op is directly inside an sde.su_iterate body
// CHECK: SU bodies are scheduling-only and may contain only direct-boundary CUs
