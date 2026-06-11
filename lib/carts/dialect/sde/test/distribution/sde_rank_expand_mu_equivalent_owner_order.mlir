// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Equivalent owner-tile writers may name loop-owner order differently while
// committing the same memory-space block grid. Rank expansion compares the
// canonical MU layout, not raw physicalOwnerDims attr order, so the MU does not
// stay flat and lose access-window materialization later.

// CHECK-LABEL: func.func @equivalent_owner_order
// CHECK: sde.mu_alloc : memref<4x4x16x16xf32>
// CHECK-NOT: sde.mu_alloc : memref<64x64xf32>
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<4x4x16x16xf32>
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<4x4x16x16xf32>

func.func @equivalent_owner_order() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %zero = arith.constant 0.000000e+00 : f32
  %A = sde.mu_alloc : memref<64x64xf32>
  sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      memref.store %zero, %A[%i, %j] : memref<64x64xf32>
      sde.yield
    }
    sde.yield
  } {physicalOwnerDims = [0, 1], physicalBlockShape = [16, 16]}
  sde.su_iterate (%c0, %c0) to (%c64, %c64) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      memref.store %zero, %A[%j, %i] : memref<64x64xf32>
      sde.yield
    }
    sde.yield
  } {physicalOwnerDims = [1, 0], physicalBlockShape = [16, 16]}
  return
}
