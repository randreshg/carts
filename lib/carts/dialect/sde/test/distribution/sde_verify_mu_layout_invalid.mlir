// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Verifier negative: a rank-expanded ("block-grid") MU whose structure is
// inconsistent with the committed plan is rejected. Here the committed plan
// declares owner dim 1 with block extent 16, but the carrier memref<8x16x64> is
// split on dim 0 (grid 8) — the owner-dim tile extent (64) does not match the
// committed block (16), so `ownerDims == recover(structure)` fails closed.

// CHECK: error: {{.*}}committed block shape on the owner dim

func.func @reject_inconsistent_block_grid_mu() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<8x16x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c8) step (%c1) classification(<elementwise>) {
    ^bb0(%bid: index):
      memref.store %cst, %A[%bid, %c0, %c0] : memref<8x16x64xf32>
      sde.yield
    } {physicalOwnerDims = [1], physicalBlockShape = [128, 16]}
    sde.yield
  }
  return
}
