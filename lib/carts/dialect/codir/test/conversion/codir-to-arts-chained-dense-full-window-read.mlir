// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=codir.codelet

module {
  func.func @chained_dense_read_from_superset_backing_grid() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %zero = arith.constant 0.000000e+00 : f64
    %tmp = memref.alloc() : memref<16x16xf64>
    %out = memref.alloc() : memref<16x16xf64>

    scf.for %i = %c0 to %c16 step %c4 {
      scf.for %j = %c0 to %c16 step %c4 {
        codir.codelet deps(%tmp : memref<16x16xf64>)
            params(%i, %j : index, index)
            attributes {array_layout = [{arrayId = 0 : i64, blockShape = [4, 4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 16 : i64, ownerDims = [0, 1], role = "write"}],
                        completion_barrier,
                        dep_array_ids = [0],
                        dep_collectives = [#codir.collective<none>],
                        dep_modes = [#codir.access_mode<readwrite>],
                        dep_owner_dims = [[0, 1]],
                        dep_storage_views = [#codir.storage_view<phase_redistributed>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        in_place_safe,
                        iteration_topology = #codir.iteration_topology<owner_tile_2d>,
                        logical_worker_slice = [4, 4],
                        pattern = #codir.pattern<matmul>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 4]} {
        ^bb0(%tmp_arg: memref<16x16xf64>, %base_i: index, %base_j: index):
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c16 = arith.constant 16 : index
          %inner_zero = arith.constant 0.000000e+00 : f64
          %end_i_raw = arith.addi %base_i, %inner_c4 : index
          %end_i = arith.minui %end_i_raw, %inner_c16 : index
          scf.for %ii = %base_i to %end_i step %inner_c1 {
            %end_j_raw = arith.addi %base_j, %inner_c4 : index
            %end_j = arith.minui %end_j_raw, %inner_c16 : index
            scf.for %jj = %base_j to %end_j step %inner_c1 {
              memref.store %inner_zero, %tmp_arg[%ii, %jj] : memref<16x16xf64>
            }
          }
          codir.yield
        }
      }
    }

    scf.for %i = %c0 to %c16 step %c4 {
      scf.for %j = %c0 to %c16 step %c4 {
        codir.codelet deps(%out, %tmp : memref<16x16xf64>, memref<16x16xf64>)
            params(%i, %j : index, index)
            attributes {array_layout = [{arrayId = 1 : i64, blockShape = [4, 4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 16 : i64, ownerDims = [0, 1], role = "write"},
                                        {arrayId = 0 : i64, blockShape = [4, 16], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0], role = "read"}],
                        completion_barrier,
                        dep_array_ids = [1, 0],
                        dep_collectives = [#codir.collective<none>, #codir.collective<none>],
                        dep_modes = [#codir.access_mode<readwrite>, #codir.access_mode<read>],
                        dep_owner_dims = [[0, 1], [0]],
                        dep_storage_views = [#codir.storage_view<phase_redistributed>, #codir.storage_view<phase_redistributed>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        in_place_safe,
                        iteration_topology = #codir.iteration_topology<owner_tile_2d>,
                        logical_worker_slice = [4, 4],
                        pattern = #codir.pattern<matmul>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [4, 4]} {
        ^bb0(%out_arg: memref<16x16xf64>, %tmp_arg: memref<16x16xf64>, %base_i: index, %base_j: index):
          %inner_c0 = arith.constant 0 : index
          %inner_c1 = arith.constant 1 : index
          %inner_c4 = arith.constant 4 : index
          %inner_c16 = arith.constant 16 : index
          %end_i_raw = arith.addi %base_i, %inner_c4 : index
          %end_i = arith.minui %end_i_raw, %inner_c16 : index
          scf.for %ii = %base_i to %end_i step %inner_c1 {
            %end_j_raw = arith.addi %base_j, %inner_c4 : index
            %end_j = arith.minui %end_j_raw, %inner_c16 : index
            scf.for %jj = %base_j to %end_j step %inner_c1 {
              scf.for %k = %inner_c0 to %inner_c16 step %inner_c1 {
                %v = memref.load %tmp_arg[%ii, %k] : memref<16x16xf64>
                memref.store %v, %out_arg[%ii, %jj] : memref<16x16xf64>
              }
            }
          }
          codir.yield
        }
      }
    }

    memref.dealloc %out : memref<16x16xf64>
    memref.dealloc %tmp : memref<16x16xf64>
    return
  }
}

// CHECK-LABEL: func.func @chained_dense_read_from_superset_backing_grid
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}planOwnerDims = [0, 1]{{.*}}planPhysicalBlockShape = [4, 4]
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}sizes[%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}]
// CHECK: arts.edt <task>
// CHECK: [[TMP_BLOCK:%[A-Za-z0-9_]+]] = arts.db_ref %{{[A-Za-z0-9_]+}}[%{{[0-9]+}}, %{{[0-9]+}}] : memref<?x?xmemref<?x?xf64>> -> memref<?x?xf64>
// CHECK: memref.load [[TMP_BLOCK]]
