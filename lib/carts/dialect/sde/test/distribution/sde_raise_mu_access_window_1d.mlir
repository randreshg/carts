// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// 1-D: memref<1024xf32> owner[0] block[256] -> memref<4x256xf32>. The single
// write-only MU gets one write window over the full grid [0, 4) with in-tile
// valid extent [256].

// CHECK-LABEL: func.func @raise_window_1d
// CHECK: sde.mu_alloc : memref<4x256xf32>
// CHECK: sde.mu_access_window write %{{.*}} : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [4] valid [256]

func.func @raise_window_1d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      memref.store %cst, %A[%i] : memref<1024xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  return
}
