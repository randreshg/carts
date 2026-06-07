// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// Diagnose: a multi-owner committed block plan (physicalOwnerDims = [0, 1]) is out
// of the single-contiguous-owner realize scope. The verifier fails closed with a
// reason rather than realize a multi-owner grid.

// CHECK: error: {{.*}}multi-owner block plan realization is not supported here

func.func @diagnose_multiowner() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<128x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    } {physicalOwnerDims = [0, 1], physicalBlockShape = [16, 16]}
    sde.yield
  }
  return
}
