// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=codir.codelet

module {
  func.func @host_whole_block_read_uses_dynamic_ref() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %out = memref.alloc() : memref<16x16xf64>
    %tmp = memref.alloc() : memref<16xf64>

    scf.for %i = %c0 to %c16 step %c4 {
      codir.codelet deps(%tmp : memref<16xf64>)
          params(%i : index)
          attributes {array_layout = [{arrayId = 1 : i64, blockShape = [4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}],
                      dep_array_ids = [1],
                      dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<readwrite>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      in_place_safe,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [4],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%vec: memref<16xf64>, %base_i: index):
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %inner_zero = arith.constant 0.000000e+00 : f64
        %end_raw = arith.addi %base_i, %inner_c4 : index
        %end = arith.minui %end_raw, %inner_c16 : index
        scf.for %ii = %base_i to %end step %inner_c1 {
          memref.store %inner_zero, %vec[%ii] : memref<16xf64>
        }
        codir.yield
      }
    }

    scf.for %i = %c0 to %c16 step %c4 {
      scf.for %j = %c0 to %c16 step %c4 {
        codir.codelet deps(%out, %tmp : memref<16x16xf64>, memref<16xf64>)
            params(%i, %j : index, index)
            attributes {array_layout = [{arrayId = 0 : i64, blockShape = [4, 4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 16 : i64, ownerDims = [0, 1], role = "write"},
                                        {arrayId = 1 : i64, blockShape = [4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "read"}],
                        dep_array_ids = [0, 1],
                        dep_collectives = [#codir.collective<none>, #codir.collective<none>],
                        dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
                        dep_owner_dims = [[0, 1], [0]],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        in_place_safe,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [4, 4],
                        pattern = #codir.pattern<uniform>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 4]} {
        ^bb0(%out_arg: memref<16x16xf64>, %tmp_arg: memref<16xf64>, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c16 = arith.constant 16 : index
          %i_end_raw = arith.addi %base_i, %inner_c4 : index
          %i_end = arith.minui %i_end_raw, %inner_c16 : index
          scf.for %ii = %base_i to %i_end step %inner_c1 {
            %j_end_raw = arith.addi %base_j, %inner_c4 : index
            %j_end = arith.minui %j_end_raw, %inner_c16 : index
            scf.for %jj = %base_j to %j_end step %inner_c1 {
              %v = memref.load %tmp_arg[%jj] : memref<16xf64>
              memref.store %v, %out_arg[%ii, %jj] : memref<16x16xf64>
            }
          }
          codir.yield
        }
      }
    }

    memref.dealloc %tmp : memref<16xf64>
    memref.dealloc %out : memref<16x16xf64>
    return
  }

  func.func @phase_redistributed_read_uses_backing_block_window() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %out = memref.alloc() : memref<16xf64>
    %tmp = memref.alloc() : memref<16xf64>

    scf.for %i = %c0 to %c16 step %c4 {
      codir.codelet deps(%tmp : memref<16xf64>)
          params(%i : index)
          attributes {array_layout = [{arrayId = 1 : i64, blockShape = [4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"}],
                      dep_array_ids = [1],
                      dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<readwrite>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      in_place_safe,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [4],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%vec: memref<16xf64>, %base_i: index):
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %inner_zero = arith.constant 0.000000e+00 : f64
        %end_raw = arith.addi %base_i, %inner_c4 : index
        %end = arith.minui %end_raw, %inner_c16 : index
        scf.for %ii = %base_i to %end step %inner_c1 {
          memref.store %inner_zero, %vec[%ii] : memref<16xf64>
        }
        codir.yield
      }
    }

    scf.for %i = %c0 to %c16 step %c4 {
      codir.codelet deps(%out, %tmp : memref<16xf64>, memref<16xf64>)
          params(%i : index)
          attributes {array_layout = [{arrayId = 2 : i64, blockShape = [4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"},
                                      {arrayId = 1 : i64, blockShape = [8], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}],
                      dep_array_ids = [2, 1],
                      dep_collectives = [#codir.collective<none>, #codir.collective<none>],
                      dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
                      dep_owner_dims = [[0], [0]],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      in_place_safe,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [4],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%out_arg: memref<16xf64>, %tmp_arg: memref<16xf64>, %base_i: index):
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %end_raw = arith.addi %base_i, %inner_c4 : index
        %end = arith.minui %end_raw, %inner_c16 : index
        scf.for %ii = %base_i to %end step %inner_c1 {
          %v = memref.load %tmp_arg[%ii] : memref<16xf64>
          memref.store %v, %out_arg[%ii] : memref<16xf64>
        }
        codir.yield
      }
    }

    memref.dealloc %tmp : memref<16xf64>
    memref.dealloc %out : memref<16xf64>
    return
  }

  func.func @phase_redistributed_owner_relative_read_uses_block_origin() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %out = memref.alloc() : memref<16xf64>
    %tmp = memref.alloc() : memref<16xf64>

    scf.for %i = %c0 to %c16 step %c4 {
      codir.codelet deps(%out, %tmp : memref<16xf64>, memref<16xf64>)
          params(%i : index)
          attributes {array_layout = [{arrayId = 2 : i64, blockShape = [4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "write"},
                                      {arrayId = 1 : i64, blockShape = [8], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}],
                      dep_array_ids = [2, 1],
                      dep_collectives = [#codir.collective<none>, #codir.collective<none>],
                      dep_modes = [#codir.access_mode<write>, #codir.access_mode<read>],
                      dep_owner_dims = [[0], [0]],
                      dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [4],
                      pattern = #codir.pattern<elementwise_pipeline>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%out_arg: memref<16xf64>, %tmp_arg: memref<16xf64>, %base_i: index):
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %end_raw = arith.addi %base_i, %inner_c4 : index
        %end = arith.minui %end_raw, %inner_c16 : index
        scf.for %ii = %base_i to %end step %inner_c1 {
          %local = arith.subi %ii, %base_i : index
          %v = memref.load %tmp_arg[%local] : memref<16xf64>
          memref.store %v, %out_arg[%local] : memref<16xf64>
        }
        codir.yield
      }
    }

    memref.dealloc %tmp : memref<16xf64>
    memref.dealloc %out : memref<16xf64>
    return
  }

  func.func @phase_redistributed_uniform_copy_uses_block_deps_for_nested_shapes() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %src = memref.alloc() : memref<16x16xf64>
    %dst = memref.alloc() : memref<16x16xf64>

    scf.for %i = %c0 to %c16 step %c4 {
      scf.for %j = %c0 to %c16 step %c8 {
        codir.codelet deps(%src, %dst : memref<16x16xf64>, memref<16x16xf64>)
            params(%c16, %i, %j : index, index, index)
            attributes {array_layout = [{arrayId = 0 : i64, blockShape = [8, 8], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"},
                                        {arrayId = 1 : i64, blockShape = [4, 8], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0, 1], role = "write"}],
                        dep_array_ids = [0, 1],
                        dep_collectives = [#codir.collective<none>, #codir.collective<none>],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_owner_dims = [[0, 1], [0, 1]],
                        dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [4, 8],
                        partition_graph = [{blockShape = [8, 8], layoutKind = "block_parallel", muBlockCount = 4 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read"},
                                           {blockShape = [4, 8], layoutKind = "owner_block", muBlockCount = 8 : i64, muId = 1 : i64, ownerDims = [0, 1], role = "write"}],
                        pattern = #codir.pattern<uniform>,
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 8]} {
        ^bb0(%src_arg: memref<16x16xf64>, %dst_arg: memref<16x16xf64>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c8 = arith.constant 8 : index
          %inner_c16 = arith.constant 16 : index
          %i_end_raw = arith.addi %base_i, %inner_c4 : index
          %i_end = arith.minui %i_end_raw, %n : index
          %j_end_raw = arith.addi %base_j, %inner_c8 : index
          %j_end = arith.minui %j_end_raw, %inner_c16 : index
          scf.for %ii = %base_i to %i_end step %inner_c1 {
            scf.for %jj = %base_j to %j_end step %inner_c1 {
              %v = memref.load %src_arg[%ii, %jj] : memref<16x16xf64>
              memref.store %v, %dst_arg[%ii, %jj] : memref<16x16xf64>
            }
          }
          codir.yield
        }
      }
    }

    memref.dealloc %dst : memref<16x16xf64>
    memref.dealloc %src : memref<16x16xf64>
    return
  }

  func.func @phase_redistributed_uniform_copy_acquires_unaligned_source_window() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c6 = arith.constant 6 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %src = memref.alloc() : memref<16x16xf64>
    %dst = memref.alloc() : memref<16x16xf64>

    scf.for %i = %c0 to %c16 step %c6 {
      scf.for %j = %c0 to %c16 step %c4 {
        codir.codelet deps(%src, %dst : memref<16x16xf64>, memref<16x16xf64>)
            params(%c16, %i, %j : index, index, index)
            attributes {array_layout = [{arrayId = 0 : i64, blockShape = [8, 8], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "read"},
                                        {arrayId = 1 : i64, blockShape = [2, 4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 32 : i64, ownerDims = [0, 1], role = "write"}],
                        dep_array_ids = [0, 1],
                        dep_collectives = [#codir.collective<none>, #codir.collective<none>],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<write>],
                        dep_owner_dims = [[0, 1], [0, 1]],
                        dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        iteration_topology = #codir.iteration_topology<owner_tile>,
                        logical_worker_slice = [6, 4],
                        partition_graph = [{blockShape = [8, 8], layoutKind = "block_parallel", muBlockCount = 4 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read"},
                                           {blockShape = [2, 4], layoutKind = "owner_block", muBlockCount = 32 : i64, muId = 1 : i64, ownerDims = [0, 1], role = "write"}],
                        pattern = #codir.pattern<uniform>,
                        repetition_structure = #codir.repetition_structure<full_timestep>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [2, 4]} {
        ^bb0(%src_arg: memref<16x16xf64>, %dst_arg: memref<16x16xf64>, %n: index, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c6 = arith.constant 6 : index
          %inner_c16 = arith.constant 16 : index
          %i_end_raw = arith.addi %base_i, %inner_c6 : index
          %i_end = arith.minui %i_end_raw, %n : index
          %j_end_raw = arith.addi %base_j, %inner_c4 : index
          %j_end = arith.minui %j_end_raw, %inner_c16 : index
          scf.for %ii = %base_i to %i_end step %inner_c1 {
            scf.for %jj = %base_j to %j_end step %inner_c1 {
              %v = memref.load %src_arg[%ii, %jj] : memref<16x16xf64>
              memref.store %v, %dst_arg[%ii, %jj] : memref<16x16xf64>
            }
          }
          codir.yield
        }
      }
    }

    memref.dealloc %dst : memref<16x16xf64>
    memref.dealloc %src : memref<16x16xf64>
    return
  }
}

// CHECK-LABEL: func.func @host_whole_block_read_uses_dynamic_ref
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}elementSizes[%{{.*}}]{{.*}}planOwnerDims = [0]{{.*}}planPhysicalBlockShape = [4]
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}sizes[%{{.*}}]
// CHECK: arts.edt <task>
// CHECK: [[RELIDX:%[A-Za-z0-9_]+]] = arith.subi %arg{{[0-9]+}}, %{{.*}} : index
// CHECK: [[BLOCK:%[A-Za-z0-9_]+]] = arith.divui [[RELIDX]], %{{.*}} : index
// CHECK: [[SELECTED:%[A-Za-z0-9_]+]] = arts.db_ref %{{[A-Za-z0-9_]+}}{{\[}}[[BLOCK]]{{\]}} : memref<?xmemref<?xf64>> -> memref<?xf64>
// CHECK: memref.load [[SELECTED]]{{\[}}%{{[A-Za-z0-9_]+}}{{\]}} : memref<?xf64>

// CHECK-LABEL: func.func @phase_redistributed_read_uses_backing_block_window
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}elementSizes[%{{.*}}]{{.*}}planOwnerDims = [0]{{.*}}planPhysicalBlockShape = [4]
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}offsets[%{{[0-9]+}}], sizes[%{{[A-Za-z0-9_]+}}]
// CHECK: arts.edt <task>
// CHECK: arts.db_ref %{{[A-Za-z0-9_]+}}[%{{[A-Za-z0-9_]+}}] : memref<?xmemref<?xf64>> -> memref<?xf64>
// CHECK: [[READ_REF:%[A-Za-z0-9_]+]] = arts.db_ref %{{[A-Za-z0-9_]+}}[%{{[A-Za-z0-9_]+}}] : memref<?xmemref<?xf64>> -> memref<?xf64>
// CHECK: scf.for %[[IV:arg[0-9]+]] =
// CHECK: [[LOCAL_INDEX:%[A-Za-z0-9_]+]] = arith.subi %[[IV]], %{{[A-Za-z0-9_]+}} : index
// CHECK: memref.load [[READ_REF]]{{\[}}[[LOCAL_INDEX]]{{\]}} : memref<?xf64>

// CHECK-LABEL: func.func @phase_redistributed_owner_relative_read_uses_block_origin
// CHECK: arts.edt <task>
// CHECK: ^bb0(%{{.*}}, %{{.*}}, %[[BASE:arg[0-9]+]]: index):
// CHECK: scf.for %[[IV:arg[0-9]+]] =
// CHECK: %[[OWNER_REL:[A-Za-z0-9_]+]] = arith.subi %[[IV]], %[[BASE]] : index
// CHECK: %[[BLOCK_REL:[A-Za-z0-9_]+]] = arith.subi %[[IV]], %{{[A-Za-z0-9_]+}} : index
// CHECK: memref.load %{{.*}}[%[[BLOCK_REL]]] : memref<?xf64>

// CHECK-LABEL: func.func @phase_redistributed_uniform_copy_uses_block_deps_for_nested_shapes
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}planOwnerDims = [0, 1]{{.*}}planPhysicalBlockShape = [8, 8]
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}planOwnerDims = [0, 1]{{.*}}planPhysicalBlockShape = [4, 8]
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>)
// CHECK: arts.db_acquire[<out>]{{.*}}partitioning(<block>)
// CHECK: arts.edt <task>
// CHECK-SAME: depPattern = #arts.dep_pattern<uniform>
// CHECK-SAME: planPhysicalBlockShape = [4, 8]

// CHECK-LABEL: func.func @phase_redistributed_uniform_copy_acquires_unaligned_source_window
// CHECK: scf.for %[[OWNER_I:arg[0-9]+]] =
// CHECK: scf.for %[[OWNER_J:arg[0-9]+]] =
// CHECK: %[[SRC_REL_BASE:[A-Za-z0-9_]+]] = arith.subi %[[OWNER_I]], %{{[A-Za-z0-9_]+}} : index
// CHECK: %[[SRC_ROW_BLOCK:[A-Za-z0-9_]+]] = arith.divui %[[SRC_REL_BASE]], %[[SRC_BLOCK_SIZE:[A-Za-z0-9_]+]] : index
// CHECK: %[[SRC_ROW_OFFSET:[A-Za-z0-9_]+]] = arith.muli %[[SRC_ROW_BLOCK]], %[[SRC_BLOCK_SIZE]] : index
// CHECK: %[[SRC_INTRA_ROW:[A-Za-z0-9_]+]] = arith.subi %[[SRC_REL_BASE]], %[[SRC_ROW_OFFSET]] : index
// CHECK: %[[SRC_COVERED_ROW:[A-Za-z0-9_]+]] = arith.addi %[[SRC_INTRA_ROW]], %{{[A-Za-z0-9_]+}} : index
// CHECK: %[[SRC_CEIL_ROW:[A-Za-z0-9_]+]] = arith.addi %[[SRC_COVERED_ROW]], %{{[A-Za-z0-9_]+}} : index
// CHECK: %[[SRC_REQUEST_ROW_RAW:[A-Za-z0-9_]+]] = arith.divui %[[SRC_CEIL_ROW]], %[[SRC_BLOCK_SIZE]] : index
// CHECK: %[[SRC_MAX_ROW_BLOCKS:[A-Za-z0-9_]+]] = arith.constant 2 : index
// CHECK: %[[SRC_REQUEST_ROW:[A-Za-z0-9_]+]] = arith.minui %[[SRC_REQUEST_ROW_RAW]], %[[SRC_MAX_ROW_BLOCKS]] : index
// CHECK: %[[SRC_REMAINING_ROWS:[A-Za-z0-9_]+]] = arith.subi %{{[A-Za-z0-9_]+}}, %[[SRC_ROW_BLOCK]] : index
// CHECK: %[[SRC_SIZE_ROWS:[A-Za-z0-9_]+]] = arith.minui %[[SRC_REMAINING_ROWS]], %[[SRC_REQUEST_ROW]] : index
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}sizes[%[[SRC_SIZE_ROWS]], %{{[A-Za-z0-9_]+}}]
// CHECK: ^bb0(%[[SRC_ARG:arg[0-9]+]]: memref<?x?xmemref<?x?xf64>>, %[[DST_ARG:arg[0-9]+]]:
// CHECK: scf.for %[[SRC_I:arg[0-9]+]] =
// CHECK: scf.for %[[SRC_J:arg[0-9]+]] =
// CHECK: %[[SRC_OFFSET_I:[A-Za-z0-9_]+]] = arith.subi %[[SRC_I]], %{{.*}} : index
// CHECK: %[[SRC_REL_I:[A-Za-z0-9_]+]] = arith.divui %[[SRC_OFFSET_I]], %{{.*}} : index
// CHECK: %[[SRC_BLOCK_I_BASE:[A-Za-z0-9_]+]] = arith.muli %[[SRC_REL_I]], %{{.*}} : index
// CHECK: %[[SRC_LOCAL_I_ORIGIN:[A-Za-z0-9_]+]] = arith.addi %{{.*}}, %[[SRC_BLOCK_I_BASE]] : index
// CHECK: %[[SRC_LOCAL_I:[A-Za-z0-9_]+]] = arith.subi %[[SRC_I]], %[[SRC_LOCAL_I_ORIGIN]] : index
// CHECK: %[[SRC_OFFSET_J:[A-Za-z0-9_]+]] = arith.subi %[[SRC_J]], %{{.*}} : index
// CHECK: %[[SRC_REL_J:[A-Za-z0-9_]+]] = arith.divui %[[SRC_OFFSET_J]], %{{.*}} : index
// CHECK: %[[SRC_REF:.*]] = arts.db_ref %[[SRC_ARG]][%[[SRC_REL_I]], %[[SRC_REL_J]]] : memref<?x?xmemref<?x?xf64>> -> memref<?x?xf64>
// CHECK: memref.load %[[SRC_REF]][%[[SRC_LOCAL_I]], %{{.*}}] : memref<?x?xf64>
