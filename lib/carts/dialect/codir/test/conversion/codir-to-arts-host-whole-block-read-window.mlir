// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
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
