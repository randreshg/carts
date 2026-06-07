// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Verifier R1 (idempotency): two sde.mu_access_window ops for the same MU in
// one cu_region is rejected (the raiser must emit exactly one per (MU, mode)).
// The MU is a hand-crafted but in-scope converted block-grid MU (memref<4x256xf32>,
// owner[0] block[256], committed writer iterating [0,1024)).

// CHECK: error: {{.*}}duplicate sde.mu_access_window

func.func @duplicate_window() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<4x256xf32>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [4] valid [256]
    sde.mu_access_window write %A : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [4] valid [256]
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      %bid = arith.divui %i, %c256 : index
      %off = arith.remui %i, %c256 : index
      memref.store %cst, %A[%bid, %off] : memref<4x256xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  return
}
