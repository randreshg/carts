// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir)' 2>&1 \
// RUN:   | %FileCheck %s

// AC-2: halo_shape must mirror the stencil reach recomputed from the signed
// access_min/max_offsets. A halo width that does not equal the ghost width of
// the committed reach (max(|min|, |max|)) is a drifted carrier and fails closed.

// CHECK: halo_shape entry #1 (2) does not mirror the stencil reach
func.func @halo_shape_diverges_from_reach() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %A = memref.alloc() : memref<4x4x8x8xf64>
  scf.for %bi = %c0 to %c4 step %c1 {
    codir.codelet deps(%A : memref<4x4x8x8xf64>) params(%bi : index)
        attributes {access_max_offsets = [1, 1],
                    access_min_offsets = [-1, -1],
                    dep_collectives = [#codir.collective<halo>],
                    dep_modes = [#codir.access_mode<read>],
                    dep_owner_dims = [[0, 1]],
                    dep_storage_views = [#codir.storage_view<compute_block>],
                    halo_shape = [1, 2],
                    iteration_topology = #codir.iteration_topology<owner_tile>,
                    spatial_dims = [0, 1],
                    tile_owner_dims = [0, 1],
                    tile_shape = [8, 8]} {
    ^bb0(%arg0: memref<4x4x8x8xf64>, %block: index):
      %z = arith.constant 0 : index
      %v = memref.load %arg0[%block, %block, %z, %z] : memref<4x4x8x8xf64>
      codir.yield
    }
  }
  return
}
