// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,convert-sde-to-codir,verify-codir)' 2>&1 | %FileCheck %s --check-prefix=SDE2CODIR --implicit-check-not=host_whole --implicit-check-not=sde.mu_access_window
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,convert-sde-to-codir,verify-codir,dep-storage-assignment,verify-codir,convert-codir-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --check-prefix=ARTS --implicit-check-not=codir.codelet --implicit-check-not=sde.mu_alloc --implicit-check-not='partitioning(<coarse>)'

// CODIR consumes the committed sde.mu_access_window over a rank-expanded
// (block-shaped) MU: the codelet dependency becomes a concrete MU subview over
// the owner-local compute block with committed grid owner dims. The ARTS
// boundary then realizes the same fact as a block DB and one-block acquire.

// SDE2CODIR-LABEL: func.func @consume_window_1d
// SDE2CODIR: %[[BLOCK:.+]] = arith.divui
// SDE2CODIR: %[[SUBVIEW:.+]] = memref.subview %{{.*}}[%[[BLOCK]], 0] [1, 256] [1, 1]
// SDE2CODIR: codir.codelet deps(%[[SUBVIEW]]
// SDE2CODIR-SAME: dep_owner_dims = {{\[\[}}0]]
// SDE2CODIR-SAME: dep_storage_views = [#codir.storage_view<compute_block>]
// SDE2CODIR: arith.subi
// SDE2CODIR: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<1x256xf32

// ARTS-LABEL: func.func @consume_window_1d
// ARTS: arts.db_alloc
// ARTS-SAME: <block>
// ARTS-SAME: elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// ARTS-SAME: planOwnerDims = [0]
// ARTS-SAME: planPhysicalBlockShape = [1, 256]
// ARTS: arts.db_acquire
// ARTS-SAME: partitioning(<block>)
// ARTS-SAME: offsets[%{{[^]]+}}]
// ARTS-SAME: sizes[%{{[^]]+}}]
// ARTS: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<?x?xf32>

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
