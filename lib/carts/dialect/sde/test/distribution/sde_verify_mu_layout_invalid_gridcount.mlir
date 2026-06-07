// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Verifier negative (grid-count soundness): the carrier memref<3x16x64> has
// the correct owner-dim tile extent (16 == committed block) but a WRONG grid
// count (3). The committed iteration domain runs [0, 128), so the only sound
// grid is ceilDiv(128, 16) = 8. The verifier must reject the bogus grid 3
// instead of trusting a logical shape reconstructed from the expanded type
// (which would tautologically accept any grid). This locks in the
// recover()-is-not-a-tautology fix.

// CHECK: error: {{.*}}grid count 3 is not ceilDiv(extent, 16)

func.func @reject_wrong_grid_count() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c128 = arith.constant 128 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<3x16x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      memref.store %cst, %A[%c0, %c0, %c0] : memref<3x16x64xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
    sde.yield
  }
  return
}
