// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-mu-access-window-sync-opt)' 2>&1 | %FileCheck %s

// Two barriers in program order. The first separates two disjoint writers of
// %A (redundant -> removed); the second orders that second writer before a
// consumer that reads the blocks it wrote (aligned RAW -> preserved). The pass
// removes only the barrier it proves redundant and keeps the justified one, so
// exactly one barrier survives, after the first two now-adjacent CUs.

// CHECK-LABEL: func.func @sync_opt_mixed
// CHECK: sde.cu_region
// CHECK: sde.cu_region
// CHECK: sde.su_barrier
// CHECK: sde.cu_region
// CHECK-NOT: sde.su_barrier
func.func @sync_opt_mixed() {
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
  sde.su_barrier
  sde.cu_region <parallel> {
    sde.mu_access_window read %A : memref<8x4xf64> owner_dims(1) block_lo [4] block_hi [8] valid [4]
    sde.yield
  }
  return
}
