// RUN: not %carts-compile %s --pass-pipeline='builtin.module(codir-halo-exchange,convert-codir-to-arts)' 2>&1 | %FileCheck %s

// A rank-expanded owner_tile compute-block stencil splits the spatial owner
// extent into a (grid, tile) index pair, so a neighbor read u[i-1] becomes
// u[divui(i-1,T)][remui(i-1,T)]. At a tile boundary the grid coord is the
// neighbor block, not the owner slice. Per-neighbor grid-block halo
// acquisition with an in-body grid-offset select is not implemented, so this
// path must fail closed instead of materializing unpopulated grid-dim halo
// storage.

// CHECK: rank-expanded owner-tile compute-block halo

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @reject_rank_expanded_owner_tile_neighbor_halo() {
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
                      distribution_kind = #codir.distribution_kind<owner_compute>,
                      halo_shape = [1, 1],
                      iteration_topology = #codir.iteration_topology<owner_tile>,
                      logical_worker_slice = [8, 8],
                      pattern = #codir.pattern<alternating_buffer_stencil>,
                      spatial_dims = [0, 1],
                      tile_owner_dims = [0, 1],
                      tile_shape = [8, 8]} {
      ^bb0(%arg0: memref<4x4x8x8xf64>, %block: index):
        %inner_c0 = arith.constant 0 : index
        %v = memref.load %arg0[%block, %block, %inner_c0, %inner_c0] : memref<4x4x8x8xf64>
        codir.yield
      }
    }
    return
  }
}
