// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-mu-access-window-sync-opt)' 2>&1 | %FileCheck %s

// No access windows have been raised, so there is no surface to prove the
// barrier redundant. The transform is conservative: it leaves ordinary
// pre-raise IR untouched rather than guessing.

// CHECK-LABEL: func.func @sync_opt_out_of_scope
// CHECK: sde.su_barrier
func.func @sync_opt_out_of_scope() {
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
