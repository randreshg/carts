// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Verifier R1 (coverage): a converted block-grid MU that is in scope but
// has no raised window is rejected. Here rank expansion converts the MUs but
// raise-to-mu-access-window is deliberately NOT run, so verify-sde-mu-access-window
// fails closed instead of silently accepting an un-raised converted MU.

// CHECK: error: {{.*}}no sde.mu_access_window in its enclosing cu_region

func.func @missing_window() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  %C = sde.mu_alloc : memref<128x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        memref.store %v, %C[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
    sde.yield
  }
  return
}
