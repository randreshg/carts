// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,convert-sde-to-codir,verify-codir)' 2>&1 | %FileCheck %s --implicit-check-not=host_whole --implicit-check-not=sde.mu_access_window

// CODIR consumes the committed sde.mu_access_window over a rank-expanded
// (block-shaped) MU: the codelet dependency becomes an owner-local compute
// block with the committed grid owner dims, instead of the coarse host_whole
// fallback, and the access-window carrier is erased at the boundary.

// CHECK-LABEL: func.func @consume_window_1d
// CHECK: codir.codelet
// CHECK-SAME: dep_owner_dims = {{\[\[}}0]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>]

func.func @consume_window_1d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
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
