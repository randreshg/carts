// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// 1-D tiled array. memref<1024xf32> with owner dim 0 and block 256 becomes
// memref<4x256xf32>; logical A[i] becomes A[i/256, i%256].

// CHECK-LABEL: func.func @rank_expand_1d
// CHECK: sde.mu_alloc : memref<4x256xf32>
// CHECK: %[[BID:.*]] = arith.divui %{{.*}}, %c256
// CHECK: %[[OFF:.*]] = arith.remui %{{.*}}, %c256
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[BID]], %[[OFF]]] : memref<4x256xf32>

func.func @rank_expand_1d() {
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
