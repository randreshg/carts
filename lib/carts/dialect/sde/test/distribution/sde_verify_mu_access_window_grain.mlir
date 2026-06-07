// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Verifier R2 (non-tautological grain): a window whose block_hi does not
// equal ceilDiv(iterationExtent, blockExtent) of any committed iteration extent
// on the writer su_iterate is rejected. Here the committed domain is [0,1024)
// with block 256, so the only sound grid is ceilDiv(1024,256)=4, but the window
// claims block_hi [3]. The check reads the iteration DOMAIN (an independent
// fact), so it cannot be satisfied by a recomputed grain. (block_hi 3 <= grid 4
// passes the op's own bounds verifier, so this exercises the pass-level grain
// proof, not the structural bound.)

// CHECK: error: {{.*}}not ceilDiv(iterationExtent

func.func @grain_mismatch() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c256 = arith.constant 256 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<4x256xf32>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [3] valid [256]
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
