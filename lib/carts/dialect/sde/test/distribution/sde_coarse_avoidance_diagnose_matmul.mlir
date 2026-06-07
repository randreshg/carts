// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// Diagnose: a matmul contraction is out of the single-writer block-grid scope and
// would need cross-tile redistribution (an op that does not exist yet). The
// verifier fails closed with a reason rather than letting coarse pass silently;
// nothing invents a redistribution op or a finer block.

// CHECK: error: {{.*}}matmul contraction needs cross-tile redistribution

func.func @diagnose_matmul() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<128x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c128) step (%c1) classification(<matmul>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
    sde.yield
  }
  return
}
