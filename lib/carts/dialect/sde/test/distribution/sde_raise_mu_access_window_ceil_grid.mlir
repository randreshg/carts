// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Ceil/partial grid: owner extent 100 is not a multiple of block 30, so the
// grid is ceilDiv(100,30)=4 and the carrier is memref<4x30x64xf32>. The window
// spans the full grid [0, 4) with uniform in-tile valid extent [30, 64]; the
// partial last block's true element count is derivable by a consumer from the
// writer su_iterate iteration domain (not recomputed here). The verifier's
// non-tautological grain check (blockHi == ceilDiv(100,30)) passes.

// CHECK-LABEL: func.func @raise_window_ceil_grid
// CHECK: sde.mu_alloc : memref<4x30x64xf32>
// CHECK: sde.mu_access_window write %{{.*}} : memref<4x30x64xf32> owner_dims(1) block_lo [0] block_hi [4] valid [30, 64]

func.func @raise_window_ceil_grid() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c100 = arith.constant 100 : index
  %cst = arith.constant 2.0 : f32
  %A = sde.mu_alloc : memref<100x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c100) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<100x64xf32>
      }
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [30, 64]}
    sde.yield
  }
  return
}
