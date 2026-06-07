// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// No access windows have been raised, so there is no surface to reason about.
// The pass is conservative: it skips the barrier rather than rejecting ordinary
// pre-raise IR. Window coverage is a separate verifier's concern.

// CHECK-LABEL: func.func @sync_out_of_scope
// CHECK: sde.su_barrier
func.func @sync_out_of_scope() {
  %c0 = arith.constant 0 : index
  %cst = arith.constant 0.0 : f64
  %A = sde.mu_alloc : memref<8xf64>
  sde.cu_region <parallel> {
    memref.store %cst, %A[%c0] : memref<8xf64>
    sde.yield
  }
  sde.su_barrier
  sde.cu_region <parallel> {
    %v = memref.load %A[%c0] : memref<8xf64>
    sde.yield
  }
  return
}
