// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// The producer writes %A blocks [0, 4) and the consumer reads blocks [2, 8):
// the windows overlap, but the consumer reaches blocks [4, 8) the producer
// never wrote. Ordering alone cannot make that data correct; the read needs a
// redistribution, which is a later SDE distribution transform.

// CHECK: error: {{.*}}redistribution
func.func @sync_misaligned() {
  %A = sde.mu_alloc : memref<8x4xf64>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<8x4xf64> owner_dims(1) block_lo [0] block_hi [4] valid [4]
    sde.yield
  }
  sde.su_barrier
  sde.cu_region <parallel> {
    sde.mu_access_window read %A : memref<8x4xf64> owner_dims(1) block_lo [2] block_hi [8] valid [4]
    sde.yield
  }
  return
}
