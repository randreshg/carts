// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir)' 2>&1 \
// RUN:   | %FileCheck %s

// AC-2 positive: the symmetric reach [-1,-1]/[1,1] mirrors halo_shape [1,1], so
// verify-codir accepts the codelet (the module round-trips unchanged).

// CHECK-NOT: does not mirror the stencil reach
// CHECK: func.func @halo_shape_matches_reach
func.func @halo_shape_matches_reach() {
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
                    halo_shape = [1, 1],
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
