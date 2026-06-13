// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window-sync)' 2>&1 | %FileCheck %s

// The producer writes %A and the consumer reads %B: no shared MU state crosses
// the barrier, so the ordering is redundant once both phases are fully
// described by raised access windows.

// CHECK: error: {{.*}}redundant
func.func @sync_redundant() {
  %A = sde.mu_alloc : memref<8x4xf64>
  %B = sde.mu_alloc : memref<8x4xf64>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<8x4xf64>
    sde.yield
  }
  sde.su_barrier
  sde.cu_region <parallel> {
    sde.mu_access_window read %B : memref<8x4xf64>
    sde.yield
  }
  return
}
