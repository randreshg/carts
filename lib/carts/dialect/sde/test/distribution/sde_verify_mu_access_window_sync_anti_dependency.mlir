// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// An earlier CU reads %A blocks [2, 4); a later CU overwrites a wider range
// [0, 8). The barrier orders the read before the overwrite (anti-dependency).
// Overlap alone justifies it: the reader sources its data elsewhere, so the
// write covering more blocks than the read needs no redistribution.

// CHECK-LABEL: func.func @sync_anti_dependency
// CHECK: sde.su_barrier
func.func @sync_anti_dependency() {
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
