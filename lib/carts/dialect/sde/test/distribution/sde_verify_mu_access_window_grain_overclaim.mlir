// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Verifier R2 (non-tautology): block_hi EQUALS the MU
// grid dim (5 == 5), so a BROKEN verifier that re-read the grain from the type
// (block_hi == muType.getShape().front()) would ACCEPT this. The real R2 reads
// the committed iteration DOMAIN: ceilDiv(1024, 256) = 4 != 5, so it rejects.
// This is exactly the case the under-claim (block_hi [3]) fixture cannot
// distinguish — together they pin the check to the iteration domain, not the type.

// CHECK: error: {{.*}}not ceilDiv(iterationExtent

func.func @grain_overclaim() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<5x256xf32>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<5x256xf32> owner_dims(1) block_lo [0] block_hi [5] valid [256]
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      %bid = arith.divui %i, %c256 : index
      %off = arith.remui %i, %c256 : index
      memref.store %cst, %A[%bid, %off] : memref<5x256xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  return
}
