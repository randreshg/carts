// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

// Grouped compute over a halo-backed block dependency needs lane-specific halo
// acquires. Until that is materialized, CODIR-to-ARTS must reject the plan.

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @reject_grouped_halo_window() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16x16x16xf32>

    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c16 step %c8 {
        scf.for %k = %c0 to %c16 step %c8 {
          codir.codelet deps(%A : memref<16x16x16xf32>) params(%i, %j, %k : index, index, index)
              attributes {access_max_offsets = [1, 0, 0],
                          access_min_offsets = [-1, 0, 0],
                          dep_collectives = [#codir.collective<halo>],
                          dep_modes = [#codir.access_mode<read>],
                          dep_owner_dims = [[0, 1, 2]],
                          dep_storage_views = [#codir.storage_view<compute_block>],
                          distribution_kind = #codir.distribution_kind<blocked>,
                          halo_shape = [1, 0, 0],
                          iteration_topology = #codir.iteration_topology<owner_tile>,
                          logical_worker_slice = [8, 8, 8],
                          pattern = #codir.pattern<stencil_tiling_nd>,
                          tile_owner_dims = [0, 1, 2],
                          tile_shape = [4, 4, 4]} {
          ^bb0(%arg0: memref<16x16x16xf32>, %iBase: index, %jBase: index, %kBase: index):
            %inner_c1 = arith.constant 1 : index
            %inner_c8 = arith.constant 8 : index
            %iEnd = arith.addi %iBase, %inner_c8 : index
            %jEnd = arith.addi %jBase, %inner_c8 : index
            %kEnd = arith.addi %kBase, %inner_c8 : index
            scf.for %ii = %iBase to %iEnd step %inner_c1 {
              scf.for %jj = %jBase to %jEnd step %inner_c1 {
                scf.for %kk = %kBase to %kEnd step %inner_c1 {
                  %im1 = arith.subi %ii, %inner_c1 : index
                  %value = memref.load %arg0[%im1, %jj, %kk] : memref<16x16x16xf32>
                }
              }
            }
            codir.yield
          }
        }
      }
    }
    return
  }
}

// CHECK: grouped planned block-local access does not stay within the block window
