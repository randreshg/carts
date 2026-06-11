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

// CHECK-LABEL: func.func @rank_expand_scalar_loaded_extent
// CHECK: sde.mu_alloc : memref<4x30xf32>
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<4x30xf32>

func.func @rank_expand_ceil_grid() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c100 = arith.constant 100 : index
  %cst = arith.constant 2.0 : f32
  %A = sde.mu_alloc : memref<100x64xf32>
  sde.su_iterate (%c0) to (%c100) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<100x64xf32>
    }
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [30, 64]}
  return
}

func.func @rank_expand_scalar_loaded_extent() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c30 = arith.constant 30 : index
  %c100_i64 = arith.constant 100 : i64
  %cst = arith.constant 1.0 : f32
  %extent = memref.alloca() : memref<i64>
  memref.store %c100_i64, %extent[] : memref<i64>
  %loaded = memref.load %extent[] : memref<i64>
  %ub = arith.index_cast %loaded : i64 to index
  %A = sde.mu_alloc : memref<100xf32>
  sde.su_iterate (%c0) to (%ub) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i] : memref<100xf32>
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [30]}
  return
}
