// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-mu-access-window-sync-opt)' 2>&1 | %FileCheck %s

// The producer writes %A blocks [0, 4) and the consumer reads blocks [2, 8):
// the consumer reaches blocks [4, 8) the producer never wrote. That is a
// movement need owned by the later SDE redistribution transform, not an
// ordering this pass may drop -- so the barrier is preserved (the transform
// fails closed and never rewrites a misaligned edge).

// CHECK-LABEL: func.func @sync_opt_misaligned
// CHECK: sde.su_barrier
func.func @sync_opt_misaligned() {
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
