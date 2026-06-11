// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// Local scratch memrefs are not SDE MUs and do not need access windows for the
// MU-window barrier proof. The barrier is justified by %A's overlapping
// write/read windows; the scratch load/store must not make it unprovable.

// CHECK-LABEL: func.func @sync_ignores_local_scratch
// CHECK: sde.su_barrier
func.func @sync_ignores_local_scratch() {
  %true = arith.constant true
  %scratch = memref.alloc() : memref<i1>
  %A = sde.mu_alloc : memref<8x4xf64>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<8x4xf64> owner_dims(1) block_lo [0] block_hi [8] valid [4]
    memref.store %true, %scratch[] : memref<i1>
    sde.yield
  }
  sde.su_barrier
  sde.cu_region <parallel> {
    sde.mu_access_window read %A : memref<8x4xf64> owner_dims(1) block_lo [0] block_hi [8] valid [4]
    %v = memref.load %scratch[] : memref<i1>
    memref.store %v, %scratch[] : memref<i1>
    sde.yield
  }
  return
}
