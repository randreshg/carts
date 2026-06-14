// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-raise-to-sde)' 2>&1 | %FileCheck %s

// Proven-independent affine.for nests are raised via isLoopParallel without
// a planning-head LowerAffine bridge.

// CHECK-LABEL: func.func @promote_affine_nest
// CHECK: sde.su_iterate
// CHECK: sde.cu_region <parallel>
// CHECK: memref.store
// CHECK-NOT: affine.for
// CHECK-NOT: affine.store
// CHECK-NOT: scf.for

func.func @promote_affine_nest(%A: memref<8x8xf32>, %v: f32) {
  affine.for %i = 0 to 8 {
    affine.for %j = 0 to 8 {
      affine.store %v, %A[%i, %j] : memref<8x8xf32>
    }
  }
  return
}
