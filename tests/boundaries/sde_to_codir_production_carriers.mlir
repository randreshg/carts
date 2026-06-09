// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from=sde-planning --pipeline=sde-planning | %FileCheck %s --check-prefix=SDE
// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from=sde-planning --pipeline=codir-graph-transforms | %FileCheck %s --check-prefix=CODIR --implicit-check-not=sde.mu_access_window

// SDE production creates a committed MU access-window carrier, and the
// SDE-to-CODIR boundary consumes it rather than letting it cross the dialect
// frontier.

// SDE-LABEL: func.func @sde_to_codir_production_carrier
// SDE: sde.mu_access_window write %{{.*}} : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [4] valid [256]

// CODIR-LABEL: func.func @sde_to_codir_production_carrier
// CODIR: memref.subview
// CODIR: codir.codelet
// CODIR-SAME: dep_array_ids = [0]
// CODIR-SAME: dep_owner_dims = {{\[\[}}0]]
// CODIR-SAME: dep_storage_views = [#codir.storage_view<compute_block>]

func.func @sde_to_codir_production_carrier() {
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
