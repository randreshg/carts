// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// A root CU can allocate an MU, schedule parallel writers as a sibling SU, then
// read the MU locally. This fixture guards the CU/SU shape and SSA dominance:
// scheduling must not be nested under the allocation CU.

// CHECK-LABEL: func.func @root_cu_local_mu_window_dominates
// CHECK:         %[[A:.*]] = sde.cu_region <single> -> (memref<64xf32>) {
// CHECK:           sde.mu_alloc : memref<64xf32>
// CHECK:         sde.su_iterate
// CHECK:           sde.cu_region
// CHECK:             memref.store %{{.*}}, %[[A]][%{{.*}}] : memref<64xf32>
// CHECK:         sde.cu_region <single> {
// CHECK:           memref.load %[[A]][%{{.*}}] : memref<64xf32>

func.func @root_cu_local_mu_window_dominates() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %one = arith.constant 1.0 : f32
  %A = sde.cu_region <single> -> (memref<64xf32>) {
    %raw = sde.mu_alloc : memref<64xf32>
    sde.yield %raw : memref<64xf32>
  }
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %one, %A[%i] : memref<64xf32>
      sde.yield
    }
    sde.yield
  } {physicalOwnerDims = [0], physicalBlockShape = [16]}
  sde.cu_region <single> {
    %v = memref.load %A[%c0] : memref<64xf32>
    sde.yield
  }
  return
}
