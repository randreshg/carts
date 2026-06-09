// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts)' 2>&1 \
// RUN:   | %FileCheck %s

// Rank-expanded owner-strip storage may group adjacent row blocks, but a halo
// read/write stencil cannot be represented by one widened whole-block acquire.
// CODIR-to-ARTS must reject until that path is split into explicit per-face
// element windows.

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @rank_expanded_owner_strip_halo_bridge() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c7 = arith.constant 7 : index
    %c8 = arith.constant 8 : index
    %A = memref.alloc() : memref<8x1x8xf64>

    scf.for %wave = %c1 to %c8 step %c1 {
      scf.for %row = %c1 to %c7 step %c4 {
        codir.codelet deps(%A : memref<8x1x8xf64>) params(%wave, %row : index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        dep_collectives = [#codir.collective<halo>],
                        dep_modes = [#codir.access_mode<readwrite>],
                        dep_owner_dims = [[0]],
                        dep_storage_views = [#codir.storage_view<compute_block>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1],
                        iteration_topology = #codir.iteration_topology<owner_strip>,
                        logical_worker_slice = [4, 8],
                        pattern = #codir.pattern<stencil_tiling_nd>,
                        plan_owner_dims = [0, 1],
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0],
                        tile_shape = [1, 8],
                        write_footprint = [1, 1]} {
        ^bb0(%arg0: memref<8x1x8xf64>, %wave_arg: index, %row_arg: index):
          %inner_c0 = arith.constant 0 : index
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c7 = arith.constant 7 : index
          %end_raw = arith.addi %row_arg, %inner_c4 : index
          %end = arith.minui %end_raw, %inner_c7 : index
          scf.for %lane = %row_arg to %end step %inner_c1 {
            %row_m = arith.subi %lane, %inner_c1 : index
            %row_p = arith.addi %lane, %inner_c1 : index
            %row_m_outer = arith.divui %row_m, %inner_c1 : index
            %row_m_inner = arith.remui %row_m, %inner_c1 : index
            %lane_outer = arith.divui %lane, %inner_c1 : index
            %lane_inner = arith.remui %lane, %inner_c1 : index
            %row_p_outer = arith.divui %row_p, %inner_c1 : index
            %row_p_inner = arith.remui %row_p, %inner_c1 : index
            %north = memref.load %arg0[%row_m_outer, %row_m_inner, %inner_c0] : memref<8x1x8xf64>
            %center = memref.load %arg0[%lane_outer, %lane_inner, %inner_c0] : memref<8x1x8xf64>
            %south = memref.load %arg0[%row_p_outer, %row_p_inner, %inner_c0] : memref<8x1x8xf64>
            %sum0 = arith.addf %north, %center : f64
            %sum1 = arith.addf %sum0, %south : f64
            memref.store %sum1, %arg0[%lane_outer, %lane_inner, %inner_c0] : memref<8x1x8xf64>
          }
          codir.yield
        }
      }
    }

    return
  }
}

// CHECK: grouped owner-compute halo dependency #0 requires explicit per-face element_offsets/element_sizes
