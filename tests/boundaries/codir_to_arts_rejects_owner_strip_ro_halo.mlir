// RUN: not %carts-compile %s --pass-pipeline='builtin.module(convert-sde-boundary-to-arts,convert-codir-to-arts)' 2>&1 | %FileCheck %s

// The SDE owner-strip RO halo carrier is projected onto the rank-expanded grid
// dimension before CODIR. Until the dedicated CODIR/ARTS materialization lands,
// CODIR-to-ARTS must reject that read-only owner-strip halo instead of
// materializing a partial exchange.

// CHECK: rank-expanded owner-strip read-only halo

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @reject_rank_expanded_owner_strip_ro_halo() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16x8x8x4xf32>
    scf.for %b = %c0 to %c16 step %c1 {
      codir.codelet deps(%A : memref<16x8x8x4xf32>) params(%b : index)
          attributes {access_max_offsets = [1, 0, 0, 0],
                      access_min_offsets = [-1, 0, 0, 0],
                      dep_collectives = [#codir.collective<halo>],
                      dep_modes = [#codir.access_mode<read>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      halo_shape = [1, 0, 0, 0],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 8, 4],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      tile_owner_dims = [2],
                      tile_shape = [8, 8, 4]} {
      ^bb0(%arg0: memref<16x8x8x4xf32>, %block: index):
        %inner_c0 = arith.constant 0 : index
        %v = memref.load %arg0[%block, %inner_c0, %inner_c0, %inner_c0] : memref<16x8x8x4xf32>
        codir.yield
      }
    }
    return
  }
}
