// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg | %FileCheck %s --implicit-check-not="stencil_read_internode_use"

// Owner-strip read-only halo deps are materialized as distributed per-block DBs
// with explicit partial-window acquires for every RO input.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @owner_strip_ro_halo_materializes_all_inputs(%vx: memref<8x8x16xf64>,
                                                         %vy: memref<8x8x16xf64>,
                                                         %vz: memref<8x8x16xf64>,
                                                         %out: memref<8x8x16xf64>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c15 = arith.constant 15 : index
    %c16 = arith.constant 16 : index

    scf.for %k = %c1 to %c15 step %c4 {
      codir.codelet deps(%vx, %vy, %vz, %out : memref<8x8x16xf64>, memref<8x8x16xf64>, memref<8x8x16xf64>, memref<8x8x16xf64>)
          params(%k : index)
          attributes {access_max_offsets = [0, 0, 1],
                      access_min_offsets = [0, 0, -1],
                      dep_collectives = [#codir.collective<halo>, #codir.collective<halo>, #codir.collective<halo>, #codir.collective<none>],
                      dep_modes = [#codir.access_mode<read>, #codir.access_mode<read>, #codir.access_mode<read>, #codir.access_mode<write>],
                      dep_owner_dims = [[2], [2], [2], [2]],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<owner_compute>,
                      halo_shape = [0, 0, 1],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 8, 4],
                      pattern = #codir.pattern<cross_dim_stencil_3d>,
                      plan_owner_dims = [2],
                      spatial_dims = [0, 1, 2],
                      tile_owner_dims = [2],
                      tile_shape = [8, 8, 4],
                      write_footprint = [1, 1, 1]} {
      ^bb0(%vx_arg: memref<8x8x16xf64>, %vy_arg: memref<8x8x16xf64>,
           %vz_arg: memref<8x8x16xf64>, %out_arg: memref<8x8x16xf64>,
           %base_k: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c15 = arith.constant 15 : index
        %end_raw = arith.addi %base_k, %inner_c4 : index
        %end = arith.minui %end_raw, %inner_c15 : index
        scf.for %kk = %base_k to %end step %inner_c1 {
          %km = arith.subi %kk, %inner_c1 : index
          %kp = arith.addi %kk, %inner_c1 : index
          %x = memref.load %vx_arg[%inner_c0, %inner_c0, %km] : memref<8x8x16xf64>
          %y = memref.load %vy_arg[%inner_c0, %inner_c0, %km] : memref<8x8x16xf64>
          %z = memref.load %vz_arg[%inner_c0, %inner_c0, %kp] : memref<8x8x16xf64>
          %xy = arith.addf %x, %y : f64
          %sum = arith.addf %xy, %z : f64
          memref.store %sum, %out_arg[%inner_c0, %inner_c0, %kk] : memref<8x8x16xf64>
        }
        codir.yield
      }
    }
    return
  }
}

// CHECK-LABEL: func.func @owner_strip_ro_halo_materializes_all_inputs
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: distributed
// CHECK-SAME: perBlockSingleWriterStencil
// CHECK-SAME: planOwnerDims = [2]
// CHECK-SAME: planPhysicalBlockShape = [8, 8, 4]
// CHECK-SAME: stencil_supported_block_halo
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}offsets[{{.*}}]{{.*}}sizes[{{.*}}]{{.*}}bounds_valid
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}offsets[{{.*}}]{{.*}}sizes[{{.*}}]{{.*}}bounds_valid
// CHECK: perBlockHaloExchange
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: distributed
// CHECK-SAME: perBlockSingleWriterStencil
// CHECK-SAME: planOwnerDims = [2]
// CHECK-SAME: planPhysicalBlockShape = [8, 8, 4]
// CHECK-SAME: stencil_supported_block_halo
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}offsets[{{.*}}]{{.*}}sizes[{{.*}}]{{.*}}bounds_valid
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}offsets[{{.*}}]{{.*}}sizes[{{.*}}]{{.*}}bounds_valid
// CHECK: perBlockHaloExchange
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: distributed
// CHECK-SAME: perBlockSingleWriterStencil
// CHECK-SAME: planOwnerDims = [2]
// CHECK-SAME: planPhysicalBlockShape = [8, 8, 4]
// CHECK-SAME: stencil_supported_block_halo
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}offsets[{{.*}}]{{.*}}sizes[{{.*}}]{{.*}}bounds_valid
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}offsets[{{.*}}]{{.*}}sizes[{{.*}}]{{.*}}bounds_valid
// CHECK: perBlockHaloExchange
// CHECK: arts.edt <task> <internode>
// CHECK-SAME: depPattern = #arts.dep_pattern<cross_dim_stencil_3d>
