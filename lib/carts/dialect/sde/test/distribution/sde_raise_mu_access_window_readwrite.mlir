// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Same-CU in-place access is executable CU work over one MU. SDE must carry it
// as a readwrite access window so ARTS can lower it to an inout dependency.

// CHECK-LABEL: func.func @raise_window_readwrite
// CHECK: sde.mu_access_window readwrite %{{.*}} : memref<8x16x64xf32> owner_dims(1) block_lo [0] block_hi [8] valid [16, 64]

func.func @raise_window_readwrite() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        %w = arith.addf %v, %v : f32
        memref.store %w, %A[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
  return
}
