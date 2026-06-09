// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_multinode_8x64.cfg | %FileCheck %s
// RUN: %carts-compile %s --pipeline post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s --check-prefix=SINGLE

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
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      dep_collectives = [#codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<blocked>,
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
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      dep_collectives = [#codir.collective<halo>],
                      distribution_kind = #codir.distribution_kind<blocked>,
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

  func.func @read_compute_block_uses_storage_block_origin() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c15 = arith.constant 15 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16x16xf32>
    %B = memref.alloc() : memref<16x16xf32>

    scf.for %i = %c1 to %c15 step %c2 {
      scf.for %j = %c1 to %c15 step %c4 {
        codir.codelet deps(%A, %B : memref<16x16xf32>, memref<16x16xf32>)
            params(%i, %j : index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        array_layout = [{arrayId = 0 : i64, blockShape = [8, 8], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"},
                                        {arrayId = 1 : i64, blockShape = [2, 4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 32 : i64, ownerDims = [0, 1], role = "write"}],
                        dep_array_ids = [0, 1],
                        dep_collectives = [#codir.collective<halo>, #codir.collective<none>],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_owner_dims = [[0, 1], [0, 1]],
                        dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [2, 4],
                        partition_graph = [{blockShape = [8, 8], edgeClass = "layout_mismatch", edgeCommBytes = 0 : i64, layoutKind = "block_parallel", muBlockCount = 4 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read", tilePayloadBytes = 256 : i64},
                                           {blockShape = [2, 4], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "owner_block", muBlockCount = 32 : i64, muId = 1 : i64, ownerDims = [0, 1], role = "write", tilePayloadBytes = 32 : i64}],
                        pattern = #codir.pattern<stencil_tiling_nd>,
                        plan_owner_dims = [0, 1],
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [2, 4],
                        write_footprint = [1, 1]} {
        ^bb0(%src: memref<16x16xf32>, %dst: memref<16x16xf32>,
             %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %row_m = arith.subi %base_i, %inner_c1 : index
          %col_p = arith.addi %base_j, %inner_c1 : index
          %a0 = memref.load %src[%row_m, %base_j] : memref<16x16xf32>
          %a1 = memref.load %src[%base_i, %col_p] : memref<16x16xf32>
          %sum = arith.addf %a0, %a1 : f32
          memref.store %sum, %dst[%base_i, %base_j] : memref<16x16xf32>
          codir.yield
        }
      }
    }

    %result = memref.load %B[%c0, %c0] : memref<16x16xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %B : memref<16x16xf32>
    memref.dealloc %A : memref<16x16xf32>
    return
  }

  func.func @read_compute_block_retiles_unaligned_halo_block() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c15 = arith.constant 15 : index
    %c16 = arith.constant 16 : index
    %A = memref.alloc() : memref<16x16xf32>
    %B = memref.alloc() : memref<16x16xf32>

    scf.for %i = %c1 to %c15 step %c2 {
      scf.for %j = %c1 to %c15 step %c4 {
        codir.codelet deps(%A, %B : memref<16x16xf32>, memref<16x16xf32>)
            params(%i, %j : index, index)
            attributes {access_max_offsets = [1, 1],
                        access_min_offsets = [-1, -1],
                        array_layout = [{arrayId = 0 : i64, blockShape = [7, 7], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 9 : i64, ownerDims = [0, 1], role = "read"},
                                        {arrayId = 1 : i64, blockShape = [2, 4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 32 : i64, ownerDims = [0, 1], role = "write"}],
                        dep_array_ids = [0, 1],
                        dep_collectives = [#codir.collective<halo>, #codir.collective<none>],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_owner_dims = [[0, 1], [0, 1]],
                        dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                        distribution_kind = #codir.distribution_kind<owner_compute>,
                        halo_shape = [1, 1],
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [2, 4],
                        partition_graph = [{blockShape = [7, 7], edgeClass = "layout_mismatch", edgeCommBytes = 0 : i64, layoutKind = "block_parallel", muBlockCount = 9 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read", tilePayloadBytes = 196 : i64},
                                           {blockShape = [2, 4], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "owner_block", muBlockCount = 32 : i64, muId = 1 : i64, ownerDims = [0, 1], role = "write", tilePayloadBytes = 32 : i64}],
                        pattern = #codir.pattern<stencil_tiling_nd>,
                        plan_owner_dims = [0, 1],
                        spatial_dims = [0, 1],
                        tile_owner_dims = [0, 1],
                        tile_shape = [2, 4],
                        write_footprint = [1, 1]} {
        ^bb0(%src: memref<16x16xf32>, %dst: memref<16x16xf32>,
             %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %row_m = arith.subi %base_i, %inner_c1 : index
          %col_p = arith.addi %base_j, %inner_c1 : index
          %a0 = memref.load %src[%row_m, %base_j] : memref<16x16xf32>
          %a1 = memref.load %src[%base_i, %col_p] : memref<16x16xf32>
          %sum = arith.addf %a0, %a1 : f32
          memref.store %sum, %dst[%base_i, %base_j] : memref<16x16xf32>
          codir.yield
        }
      }
    }

    %result = memref.load %B[%c0, %c0] : memref<16x16xf32>
    func.call @use(%result) : (f32) -> ()
    memref.dealloc %B : memref<16x16xf32>
    memref.dealloc %A : memref<16x16xf32>
    return
  }

  func.func private @use(f32)
}

// CHECK-LABEL: func.func @per_block_single_writer_stencil
// CHECK-DAG: %[[ZERO:.*]] = arith.constant 0 : index
// CHECK-DAG: %[[ONE:.*]] = arith.constant 1 : index
// CHECK-DAG: %[[EIGHT:.*]] = arith.constant 8 : index
// CHECK-DAG: %[[NINE:.*]] = arith.constant 9 : index
// CHECK-DAG: %[[TEN:.*]] = arith.constant 10 : index
// CHECK-DAG: %[[FOUR:.*]] = arith.constant 4 : index

// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <block>]
// CHECK-SAME: elementSizes[%[[TEN]], %[[FOUR]]]
// CHECK-SAME: perBlockSingleWriterStencil
// CHECK-SAME: planHaloShape = [1, 0]
// CHECK-SAME: stencil_supported_block_halo

// CHECK: scf.for
// CHECK: arts.db_acquire[<inout>] {{.*}} partitioning(<block>)
// CHECK-NOT: element_offsets
// CHECK: %[[LOWER_OK:.*]] = arith.cmpi ugt
// CHECK: %[[UPPER_OK:.*]] = arith.cmpi ult
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}bounds_valid(%[[LOWER_OK]]) element_offsets[%[[EIGHT]], %[[ZERO]]] element_sizes[%[[ONE]], %[[FOUR]]]
// CHECK: arts.db_acquire[<in>] {{.*}} partitioning(<block>){{.*}}bounds_valid(%[[UPPER_OK]]) element_offsets[%[[ONE]], %[[ZERO]]] element_sizes[%[[ONE]], %[[FOUR]]]
// CHECK: %[[TOTAL_NODES:.*]] = arts.runtime_query <total_nodes> -> i32
// CHECK: %[[NODES_IDX:.*]] = arith.index_cast %[[TOTAL_NODES]] : i32 to index
// CHECK: %[[SCALED:.*]] = arith.muli %{{.*}}, %[[NODES_IDX]] : index
// CHECK: %[[ROUTE_IDX:.*]] = arith.divui %[[SCALED]], %{{.*}} : index
// CHECK: %[[ROUTE:.*]] = arith.index_cast %[[ROUTE_IDX]] : index to i32
// CHECK: arts.edt <task> <internode> route(%[[ROUTE]])
// CHECK-SAME: perBlockHaloExchange
// CHECK-SAME: storageBridgeCopy
// CHECK: scf.if
// CHECK: memref.load %{{.*}}[%[[ZERO]], %{{.*}}]
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[ZERO]], %{{.*}}]
// CHECK: scf.if
// CHECK: memref.load %{{.*}}[%[[ZERO]], %{{.*}}]
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[NINE]], %{{.*}}]

// CHECK: arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
// CHECK: arts.edt <task> <internode> route{{.*}}depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK: %[[COMPUTED:.*]] = arith.mulf
// CHECK: %[[STORE_ORIGIN:.*]] = arith.subi %{{.*}}, %[[ONE]]
// CHECK-NOT: arith.select
// CHECK: %[[STORE_ROW:.*]] = arith.subi %{{.*}}, %[[STORE_ORIGIN]]
// CHECK: memref.store %[[COMPUTED]], %{{.*}}[%[[STORE_ROW]], %{{.*}}]

// CHECK-LABEL: func.func @per_block_halo_groups_contiguous_owner_routes
// CHECK-DAG: %[[ZERO_G:.*]] = arith.constant 0 : index
// CHECK-DAG: %[[ONE_G:.*]] = arith.constant 1 : index
// CHECK-DAG: %[[TWO_G:.*]] = arith.constant 2 : index
// CHECK-DAG: %[[FOUR_G:.*]] = arith.constant 4 : index
// CHECK-DAG: %[[EIGHT_G:.*]] = arith.constant 8 : index
// CHECK-DAG: %[[SIXTEEN_G:.*]] = arith.constant 16 : index
// CHECK: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK: scf.for %{{.*}} = %[[ZERO_G]] to %[[SIXTEEN_G]] step %[[TWO_G]]
// CHECK: attributes {storageBridgeCopy}
// CHECK: scf.for %[[BLOCK_BASE:.*]] = %[[ZERO_G]] to %[[SIXTEEN_G]] step %[[TWO_G]]
// CHECK: %[[LANE1_RAW:.*]] = arith.addi %[[BLOCK_BASE]], %[[ONE_G]] : index
// CHECK: %[[LANE1:.*]] = arith.remui %[[LANE1_RAW]], %[[SIXTEEN_G]] : index
// CHECK: arts.db_acquire[<inout>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: bounds_valid
// CHECK-SAME: element_offsets[%[[EIGHT_G]], %[[ZERO_G]]]
// CHECK-SAME: element_sizes[%[[ONE_G]], %[[FOUR_G]]]
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: bounds_valid
// CHECK-SAME: element_offsets[%[[ONE_G]], %[[ZERO_G]]]
// CHECK-SAME: element_sizes[%[[ONE_G]], %[[FOUR_G]]]
// CHECK: %[[SCALED_G:.*]] = arith.muli %[[BLOCK_BASE]], %[[EIGHT_G]] : index
// CHECK: %[[ROUTE_IDX_G:.*]] = arith.divui %[[SCALED_G]], %[[SIXTEEN_G]] : index
// CHECK: %[[ROUTE_G:.*]] = arith.index_cast %[[ROUTE_IDX_G]] : index to i32
// CHECK: arts.edt <task> <internode> route(%[[ROUTE_G]]) (%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}) :
// CHECK-SAME: perBlockHaloExchange
// CHECK-SAME: storageBridgeCopy
// CHECK: memref.store
// CHECK: memref.store

// SINGLE: arts.runtime_total_nodes = 1 : i64
// SINGLE-LABEL: func.func @per_block_single_writer_stencil
// SINGLE: arts.db_acquire[<in>] {{.*}} element_offsets
// SINGLE-SAME: element_sizes
// SINGLE: arts.edt <task> <intranode> route
// SINGLE-SAME: perBlockHaloExchange
// SINGLE-SAME: storageBridgeCopy

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

// CHECK-LABEL: func.func @read_compute_block_uses_storage_block_origin
// CHECK-DAG: %[[ONE_R:.*]] = arith.constant 1 : index
// CHECK-DAG: %[[EIGHT_R:.*]] = arith.constant 8 : index
// CHECK: arts.db_alloc
// CHECK-SAME: planPhysicalBlockShape = [8, 8]
// CHECK: arts.edt <task>
// CHECK-SAME: depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK: %[[READ_RELATIVE_I:.*]] = arith.subi %{{.*}}, %[[ONE_R]]
// CHECK: %[[READ_BLOCK_I:.*]] = arith.divui %[[READ_RELATIVE_I]], %[[EIGHT_R]]
// CHECK: %[[READ_OFFSET_I:.*]] = arith.muli %[[READ_BLOCK_I]], %[[EIGHT_R]]
// CHECK-NOT: arith.select
// CHECK: arith.subi %{{.*}}, %[[READ_OFFSET_I]]

// CHECK-LABEL: func.func @read_compute_block_retiles_unaligned_halo_block
// CHECK: arts.db_alloc[<in>, <heap>, <read>, <block>]
// CHECK-SAME: planPhysicalBlockShape = [2, 4]
// CHECK-SAME: stencil_supported_block_halo
// CHECK: arts.edt <task>
// CHECK-SAME: depPattern = #arts.dep_pattern<stencil_tiling_nd>
// CHECK-SAME: planPhysicalBlockShape = [2, 4]

// func.func private @use
