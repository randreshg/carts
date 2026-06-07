// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// Two CUs write disjoint halves of %A's block grid with a barrier between them.
// Their windows prove the writes never touch the same block, so the work is
// independent and the barrier is redundant.

// CHECK: error: {{.*}}redundant
func.func @sync_redundant() {
  %A = sde.mu_alloc : memref<8x4xf64>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<8x4xf64> owner_dims(1) block_lo [0] block_hi [4] valid [4]
    sde.yield
  }
  sde.su_barrier
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<8x4xf64> owner_dims(1) block_lo [4] block_hi [8] valid [4]
    sde.yield
  }
  return
}
