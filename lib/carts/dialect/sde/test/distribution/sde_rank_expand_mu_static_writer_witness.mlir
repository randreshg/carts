// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout,raise-to-mu-access-window,verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// An MU written by TWO su_iterate writers that committed the IDENTICAL block
// plan: a first writer with a fully-static iteration domain ([0, 1024)) and a
// second writer whose upper bound is a dynamic source parameter (%n) the
// frontend never propagated to the constant 1024 it actually equals. The
// grid-count proof (verify-sde-mu-layout / verify-sde-mu-access-window) must use
// the static writer as the non-tautological witness for the committed extent;
// it must not fail closed just because a different writer of the same plan has
// a dynamic bound. Both writers agree on owner dims [0] and block [256], so the
// committed grain is unambiguous either way.

// CHECK-LABEL: func.func @static_writer_witnesses_grid
// CHECK: sde.mu_alloc : memref<4x256xf32>
// Both the static init store and the dynamic compute store localize to the same
// rank-expanded block-grid coordinate system.
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<4x256xf32>
// CHECK: sde.mu_access_window write %{{.*}} : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [4] valid [256]

func.func @static_writer_witnesses_grid(%n: index) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
  // Dynamic-domain writer FIRST (the order the use-list hands the planner the
  // dynamic writer ahead of the static one): upper bound is the un-propagated
  // source extent %n (== 1024 at runtime, but not foldable). On its own this
  // writer's iteration domain cannot witness the committed grid count.
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%n) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      memref.store %cst, %A[%i] : memref<1024xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  // Static-domain writer of the SAME committed plan: literal [0, 1024). This is
  // the witness the grid-count proof must fall back to.
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      memref.store %cst, %A[%i] : memref<1024xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  return
}
