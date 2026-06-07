// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// The before phase carries a window on %A, but the after CU reads %B with no
// window. The windows present do not describe the ordered accesses, so the pass
// cannot prove the barrier independent and fails closed instead of guessing.

// CHECK: error: {{.*}}no window describes
func.func @sync_unwindowed_access() {
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
