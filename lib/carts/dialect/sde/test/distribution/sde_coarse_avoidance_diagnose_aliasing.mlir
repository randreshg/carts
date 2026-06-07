// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// Diagnose: an aliasing use of the MU root (a memref.cast) is not localizable by
// the block-grid rewriter, and with no committed block plan no grid can be
// realized. The verifier fails closed with a reason.

// CHECK: error: {{.*}}unsupported/aliasing use of the MU root blocks block-grid

func.func @diagnose_aliasing() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<128x64xf32>
  %alias = memref.cast %A : memref<128x64xf32> to memref<?x?xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    }
    sde.yield
  }
  return
}
