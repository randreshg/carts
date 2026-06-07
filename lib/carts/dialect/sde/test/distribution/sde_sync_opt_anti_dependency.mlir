// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-mu-access-window-sync-opt)' 2>&1 | %FileCheck %s

// An earlier CU reads %A blocks [2, 4); a later CU overwrites a wider range
// [0, 8). The barrier orders the read before the overwrite (anti-dependency):
// overlap alone justifies it, so the transform preserves the barrier even
// though the write covers more blocks than the read.

// CHECK-LABEL: func.func @sync_opt_anti_dependency
// CHECK: sde.su_barrier
func.func @sync_opt_anti_dependency() {
  %A = sde.mu_alloc : memref<8x4xf64>
  sde.cu_region <parallel> {
    sde.mu_access_window read %A : memref<8x4xf64> owner_dims(1) block_lo [2] block_hi [4] valid [4]
    sde.yield
  }
  sde.su_barrier
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<8x4xf64> owner_dims(1) block_lo [0] block_hi [8] valid [4]
    sde.yield
  }
  return
}
