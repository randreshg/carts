// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-mu-access-window-sync-opt)' 2>&1 | %FileCheck %s

// The before phase carries a window on %A, but the after CU reads %B with no
// window. The windows present do not describe the ordered accesses, so the pass
// cannot prove the barrier redundant and preserves it (the absence of a proven
// conflict is not a proof of independence).

// CHECK-LABEL: func.func @sync_opt_unwindowed_access
// CHECK: sde.su_barrier
func.func @sync_opt_unwindowed_access() {
  %c0 = arith.constant 0 : index
  %A = sde.mu_alloc : memref<8x4xf64>
  %B = sde.mu_alloc : memref<8xf64>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<8x4xf64> owner_dims(1) block_lo [0] block_hi [8] valid [4]
    sde.yield
  }
  sde.su_barrier
  sde.cu_region <parallel> {
    %v = memref.load %B[%c0] : memref<8xf64>
    sde.yield
  }
  return
}
