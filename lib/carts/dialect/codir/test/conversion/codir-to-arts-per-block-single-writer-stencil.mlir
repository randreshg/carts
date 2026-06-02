// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg --distributed-db | %FileCheck %s

// `halo` lowers to a distributed per-block DB plus owner-routed neighbor
// exchange EDTs.

module attributes {arts.runtime_total_nodes = 8 : i64, arts.runtime_total_workers = 512 : i64} {
  func.func @per_block_single_writer_stencil() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c24 = arith.constant 24 : index
    %A = memref.alloc() : memref<24x4xf32>

    scf.for %i = %c0 to %c24 step %c8 {
      codir.codelet deps(%A : memref<24x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      dep_collectives = [#codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      emit_block_native_stencil,
                      halo_shape = [1, 0],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<24x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c23 = arith.constant 23 : index
        %cst = arith.constant 2.000000e-01 : f32
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c23 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c1 to %inner_c4 step %inner_c1 {
            %up = arith.subi %row, %inner_c1 : index
            %dn = arith.addi %row, %inner_c1 : index
            %a0 = memref.load %arg0[%row, %col] : memref<24x4xf32>
            %a1 = memref.load %arg0[%up, %col] : memref<24x4xf32>
            %a2 = memref.load %arg0[%dn, %col] : memref<24x4xf32>
            %s0 = arith.addf %a0, %a1 : f32
            %s1 = arith.addf %s0, %a2 : f32
            %r = arith.mulf %s1, %cst : f32
            memref.store %r, %arg0[%row, %col] : memref<24x4xf32>
          }
        }
        codir.yield
      }
    }

    %resultA = memref.load %A[%c0, %c0] : memref<24x4xf32>
    func.call @use(%resultA) : (f32) -> ()
    memref.dealloc %A : memref<24x4xf32>
    return
  }

  func.func @per_block_halo_groups_contiguous_owner_routes() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c128 = arith.constant 128 : index
    %A = memref.alloc() : memref<128x4xf32>

    scf.for %i = %c0 to %c128 step %c8 {
      codir.codelet deps(%A : memref<128x4xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      dep_collectives = [#codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      emit_block_native_stencil,
                      halo_shape = [1, 0],
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [8, 4],
                      pattern = #codir.pattern<stencil_tiling_nd>,
                      tile_owner_dims = [0],
                      tile_shape = [8, 4]} {
      ^bb0(%arg0: memref<128x4xf32>, %base: index):
        %inner_c0 = arith.constant 0 : index
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c8 = arith.constant 8 : index
        %inner_c127 = arith.constant 127 : index
        %cst = arith.constant 2.000000e-01 : f32
        %end_raw = arith.addi %base, %inner_c8 : index
        %end = arith.minui %end_raw, %inner_c127 : index
        scf.for %row = %base to %end step %inner_c1 {
          scf.for %col = %inner_c1 to %inner_c4 step %inner_c1 {
            %up = arith.subi %row, %inner_c1 : index
            %dn = arith.addi %row, %inner_c1 : index
            %a0 = memref.load %arg0[%row, %col] : memref<128x4xf32>
            %a1 = memref.load %arg0[%up, %col] : memref<128x4xf32>
            %a2 = memref.load %arg0[%dn, %col] : memref<128x4xf32>
            %s0 = arith.addf %a0, %a1 : f32
            %s1 = arith.addf %s0, %a2 : f32
            %r = arith.mulf %s1, %cst : f32
            memref.store %r, %arg0[%row, %col] : memref<128x4xf32>
          }
        }
        codir.yield
      }
    }

    %resultA = memref.load %A[%c0, %c0] : memref<128x4xf32>
    func.call @use(%resultA) : (f32) -> ()
    memref.dealloc %A : memref<128x4xf32>
    return
  }

  func.func @read_only_compute_block_stencil_dep_bridges_and_routes() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %cst = arith.constant 1.000000e+00 : f32
    %F = memref.alloc() : memref<16x8xf32>
    %U = memref.alloc() : memref<16x8xf32>
    %UN = memref.alloc() : memref<16x8xf32>
    memref.store %cst, %F[%c0, %c0] : memref<16x8xf32>

    scf.for %i = %c0 to %c16 step %c8 {
      scf.for %j = %c0 to %c8 step %c4 {
        codir.codelet deps(%F, %UN, %U : memref<16x8xf32>, memref<16x8xf32>, memref<16x8xf32>)
            params(%c16, %i, %j : index, index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        dep_collectives = [#codir.collective<none>, #codir.collective<none>, #codir.collective<halo>],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>, #codir.access_mode<read>],
                        dep_owner_dims = [[0, 1], [0, 1], [0, 1]],
                        dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [8, 4],
                        pattern = #codir.pattern<alternating_buffer_stencil>,
                        plan_owner_dims = [0, 1],
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [8, 4],
                        write_footprint = [1, 1]} {
        ^bb0(%forcing: memref<16x8xf32>, %dst: memref<16x8xf32>,
             %src: memref<16x8xf32>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %row_m = arith.subi %base_i, %inner_c1 : index
          %row_p = arith.addi %base_i, %inner_c1 : index
          %col_m = arith.subi %base_j, %inner_c1 : index
          %col_p = arith.addi %base_j, %inner_c1 : index
          %f0 = memref.load %forcing[%base_i, %base_j] : memref<16x8xf32>
          %n0 = memref.load %src[%row_m, %base_j] : memref<16x8xf32>
          %s0 = memref.load %src[%row_p, %base_j] : memref<16x8xf32>
          %w0 = memref.load %src[%base_i, %col_m] : memref<16x8xf32>
          %e0 = memref.load %src[%base_i, %col_p] : memref<16x8xf32>
          %sum0 = arith.addf %n0, %s0 : f32
          %sum1 = arith.addf %w0, %e0 : f32
          %sum2 = arith.addf %sum0, %sum1 : f32
          %sum = arith.addf %sum2, %f0 : f32
          memref.store %sum, %dst[%base_i, %base_j] : memref<16x8xf32>
          codir.yield
        }
      }
    }

    %result = memref.load %UN[%c0, %c0] : memref<16x8xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %UN : memref<16x8xf32>
    memref.dealloc %U : memref<16x8xf32>
    memref.dealloc %F : memref<16x8xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @per_block_single_writer_stencil
// CHECK-DAG: %[[C0:.*]] = arith.constant 0 : index
// CHECK-DAG: %[[C1:.*]] = arith.constant 1 : index
// CHECK-DAG: %[[C8:.*]] = arith.constant 8 : index
// CHECK-DAG: %[[C9:.*]] = arith.constant 9 : index
// CHECK-DAG: %[[C10:.*]] = arith.constant 10 : index
// CHECK-DAG: %[[C4:.*]] = arith.constant 4 : index

// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: elementSizes[%[[C10]], %[[C4]]]
// CHECK-SAME: perBlockSingleWriterStencil
// CHECK-SAME: planHaloShape = [1, 0]
// CHECK-SAME: stencil_supported_block_halo

// CHECK: scf.for
// CHECK: arts.db_acquire[<out>] {{.*}} partitioning(<block>)
// CHECK: %[[LOWER_OK:.*]] = arith.cmpi ugt
// CHECK: %[[UPPER_OK:.*]] = arith.cmpi ult
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}bounds_valid(%[[LOWER_OK]]) element_offsets[%[[C8]], %[[C0]]] element_sizes[%[[C1]], %[[C4]]]
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}bounds_valid(%[[UPPER_OK]]) element_offsets[%[[C1]], %[[C0]]] element_sizes[%[[C1]], %[[C4]]]
// CHECK: %[[TOTAL_NODES:.*]] = arts.runtime_query <total_nodes> -> i32
// CHECK: %[[NODES_IDX:.*]] = arith.index_cast %[[TOTAL_NODES]] : i32 to index
// CHECK: %[[SCALED:.*]] = arith.muli %{{.*}}, %[[NODES_IDX]] : index
// CHECK: %[[ROUTE_IDX:.*]] = arith.divui %[[SCALED]], %{{.*}} : index
// CHECK: %[[ROUTE:.*]] = arith.index_cast %[[ROUTE_IDX]] : index to i32
// CHECK: arts.edt <task> <internode> route(%[[ROUTE]])
// CHECK-SAME: perBlockHaloExchange
// CHECK-SAME: storageBridgeCopy
// CHECK: scf.if
// CHECK: memref.load %{{.*}}[%[[C0]], %{{.*}}]
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[C0]], %{{.*}}]
// CHECK: scf.if
// CHECK: memref.load %{{.*}}[%[[C0]], %{{.*}}]
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[C9]], %{{.*}}]
// CHECK: arts.barrier

// CHECK: arts.edt <task> <internode> route{{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK: %[[COMPUTED:.*]] = arith.mulf
// CHECK: %[[STORE_ORIGIN_OK:.*]] = arith.cmpi uge
// CHECK: %[[STORE_ORIGIN_RAW:.*]] = arith.subi
// CHECK: %[[STORE_ORIGIN:.*]] = arith.select %[[STORE_ORIGIN_OK]], %[[STORE_ORIGIN_RAW]], %[[C0]]
// CHECK: %[[STORE_ROW:.*]] = arith.subi %{{.*}}, %[[STORE_ORIGIN]]
// CHECK: memref.store %[[COMPUTED]], %{{.*}}[%[[STORE_ROW]], %{{.*}}]

// CHECK-LABEL: func.func @per_block_halo_groups_contiguous_owner_routes
// CHECK-DAG: %[[C0_G:.*]] = arith.constant 0 : index
// CHECK-DAG: %[[C1_G:.*]] = arith.constant 1 : index
// CHECK-DAG: %[[C2_G:.*]] = arith.constant 2 : index
// CHECK-DAG: %[[C8_G:.*]] = arith.constant 8 : index
// CHECK-DAG: %[[C16_G:.*]] = arith.constant 16 : index
// CHECK: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK: scf.for %{{.*}} = %[[C0_G]] to %[[C16_G]] step %[[C2_G]]
// CHECK: attributes {critical_path_distance = 0 : i64, storageBridgeCopy}
// CHECK: scf.for %[[BLOCK_BASE:.*]] = %[[C0_G]] to %[[C16_G]] step %[[C2_G]]
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: %[[LANE1:.*]] = arith.addi %[[BLOCK_BASE]], %[[C1_G]] : index
// CHECK-NOT: arts.db_acquire[<in>]
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: bounds_valid
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: bounds_valid
// CHECK: %[[SCALED_G:.*]] = arith.muli %[[BLOCK_BASE]], %[[C8_G]] : index
// CHECK: %[[ROUTE_IDX_G:.*]] = arith.divui %[[SCALED_G]], %[[C16_G]] : index
// CHECK: %[[ROUTE_G:.*]] = arith.index_cast %[[ROUTE_IDX_G]] : index to i32
// CHECK: arts.edt <task> <internode> route(%[[ROUTE_G]]) (%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}) :
// CHECK-SAME: perBlockHaloExchange
// CHECK-SAME: storageBridgeCopy
// CHECK: memref.store
// CHECK: memref.store

// CHECK-LABEL: func.func @read_only_compute_block_stencil_dep_bridges_and_routes
// CHECK: arts.db_alloc[<in>, <heap>, <read>, <coarse>]
// CHECK-SAME: local_only
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: distributed
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK-SAME: planPhysicalBlockShape = [8, 4]
// CHECK-SAME: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: arts.edt <task> <internode> route{{.*}}memref<?xmemref<?x?xf32>>, memref<?x?xmemref<?x?xf32>>{{.*}}storageBridgeCopy
// CHECK: arts.edt <task> <intranode> route{{.*}}memref<?x?xmemref<?x?xf32>>, memref<?x?xmemref<?x?xf32>>, memref<?x?xmemref<?x?xf32>>{{.*}}depPattern = #arts.dep_pattern<alternating_buffer_stencil>
// CHECK-SAME: planOwnerDims = [0, 1]

// func.func private @use
