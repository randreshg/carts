// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-mu-access-window-sync-opt)' 2>&1 | %FileCheck %s

// A producer CU writes the full block grid of %A and a consumer CU reads the
// same blocks, ordered by a barrier (aligned RAW). The barrier orders a real
// dependency, so the transform preserves it; CODIR refines it into explicit
// dependency edges from the same windows.

// CHECK-LABEL: func.func @sync_opt_justified
// CHECK: sde.su_barrier
func.func @sync_opt_justified() {
  %A = sde.mu_alloc : memref<8x4xf64>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<8x4xf64> owner_dims(1) block_lo [0] block_hi [8] valid [4]
    sde.yield
  }
  sde.su_barrier
  sde.cu_region <parallel> {
    sde.mu_access_window read %A : memref<8x4xf64> owner_dims(1) block_lo [0] block_hi [8] valid [4]
    sde.yield
  }
  return
}
