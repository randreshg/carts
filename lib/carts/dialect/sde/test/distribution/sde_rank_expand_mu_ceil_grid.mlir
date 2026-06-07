// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Valid-range / padded grid: the owner extent (100) is NOT a multiple of the
// block (30). The grid is ceilDiv(100, 30) = 4, so the carrier is
// memref<4x30x64xf32> (the last block is partially valid). The owner index
// %i in [0, 100) maps to [%i/30, %i%30] which is in-range by construction (the
// su_iterate clamps the iteration to the logical extent); no runtime guard op is
// introduced and the recover proof still holds (verify-sde-mu-layout passes).

// CHECK-LABEL: func.func @rank_expand_ceil_grid
// CHECK: sde.mu_alloc : memref<4x30x64xf32>
// CHECK: %[[BID:.*]] = arith.divui %{{.*}}, %c30
// CHECK: %[[OFF:.*]] = arith.remui %{{.*}}, %c30
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[BID]], %[[OFF]], %{{.*}}] : memref<4x30x64xf32>

func.func @rank_expand_ceil_grid() {
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
