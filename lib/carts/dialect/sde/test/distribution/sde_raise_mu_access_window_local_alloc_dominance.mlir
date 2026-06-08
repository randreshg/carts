// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// A root CU can allocate a rank-expanded MU, schedule parallel writers, then
// read the MU locally. The root read window must be inserted after the local
// sde.mu_alloc it names; otherwise MLIR dominance rejects the SDE IR before
// CODIR can consume the window facts.

// CHECK-LABEL: func.func @root_cu_local_mu_window_dominates
// CHECK:         sde.cu_region <single> {
// CHECK:           %[[A:.*]] = sde.mu_alloc : memref<4x16xf32>
// CHECK:           sde.mu_access_window read %[[A]] : memref<4x16xf32> owner_dims(1) block_lo [0] block_hi [4] valid [16]
// CHECK:           sde.cu_region <parallel> {
// CHECK:             sde.su_iterate
// CHECK:               sde.cu_region
// CHECK:                 sde.mu_access_window write %[[A]] : memref<4x16xf32> owner_dims(1) block_lo [0] block_hi [4] valid [16]

func.func @root_cu_local_mu_window_dominates() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %one = arith.constant 1.0 : f32
  sde.cu_region <single> {
    %A = sde.mu_alloc : memref<64xf32>
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
      ^bb0(%i: index):
        sde.cu_region <single> {
          memref.store %one, %A[%i] : memref<64xf32>
          sde.yield
        }
        sde.yield
      } {physicalOwnerDims = [0], physicalBlockShape = [16]}
      sde.yield
    }
    %v = memref.load %A[%c0] : memref<64xf32>
    sde.yield
  }
  return
}
