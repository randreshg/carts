// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment,verify-codir,convert-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s

module {
  func.func @matmul_inputs_use_committed_dep_block_shapes() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c128 = arith.constant 128 : index
    %cst = arith.constant 0.000000e+00 : f64
    %a = memref.alloc() : memref<128x128xf64>
    %b = memref.alloc() : memref<128x128xf64>
    %c = memref.alloc() : memref<128x128xf64>
    scf.for %i = %c0 to %c128 step %c16 {
      scf.for %j = %c0 to %c128 step %c16 {
        codir.codelet deps(%a, %b, %c : memref<128x128xf64>, memref<128x128xf64>, memref<128x128xf64>)
            params(%i, %j : index, index)
            attributes {array_layout = [{arrayId = 1 : i64, blockShape = [64, 128], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "read"},
                                        {arrayId = 2 : i64, blockShape = [128, 64], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [1], role = "read"},
                                        {arrayId = 0 : i64, blockShape = [64, 64], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}],
                        dep_array_ids = [1, 2, 0],
                        dep_modes = [#codir.access_mode<read>, #codir.access_mode<read>, #codir.access_mode<readwrite>],
                        dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                        distribution_kind = #codir.distribution_kind<blocked>,
                        in_place_safe,
                        iteration_topology = #codir.iteration_topology<owner_tile_2d>,
                        logical_worker_slice = [16, 16],
                        partition_graph = [{blockShape = [64, 64], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "block_parallel", muBlockCount = 4 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "read", tilePayloadBytes = 32768 : i64},
                                           {blockShape = [64, 128], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "block_parallel", muBlockCount = 2 : i64, muId = 1 : i64, ownerDims = [0], role = "read", tilePayloadBytes = 65536 : i64},
                                           {blockShape = [128, 64], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "block_parallel", muBlockCount = 2 : i64, muId = 2 : i64, ownerDims = [1], role = "read", tilePayloadBytes = 65536 : i64},
                                           {blockShape = [16, 16], edgeClass = "aligned", edgeCommBytes = 0 : i64, layoutKind = "owner_block", muBlockCount = 64 : i64, muId = 0 : i64, ownerDims = [0, 1], role = "write", tilePayloadBytes = 2048 : i64}],
                        pattern = #codir.pattern<matmul>,
                        tile_owner_dims = [0, 1],
                        tile_shape = [16, 16]} {
        ^bb0(%arg0: memref<128x128xf64>, %arg1: memref<128x128xf64>, %arg2: memref<128x128xf64>, %base_i: index, %base_j: index):
          %inner_c0 = arith.constant 0 : index
          %inner_c1 = arith.constant 1 : index
          %inner_c16 = arith.constant 16 : index
          %inner_c128 = arith.constant 128 : index
          %inner_cst = arith.constant 0.000000e+00 : f64
          %i_end_raw = arith.addi %base_i, %inner_c16 : index
          %i_end = arith.minui %i_end_raw, %inner_c128 : index
          scf.for %row = %base_i to %i_end step %inner_c1 {
            %j_end_raw = arith.addi %base_j, %inner_c16 : index
            %j_end = arith.minui %j_end_raw, %inner_c128 : index
            scf.for %col = %base_j to %j_end step %inner_c1 {
              memref.store %inner_cst, %arg2[%row, %col] : memref<128x128xf64>
              scf.for %k = %inner_c0 to %inner_c128 step %inner_c1 {
                %lhs = memref.load %arg0[%row, %k] : memref<128x128xf64>
                %rhs = memref.load %arg1[%k, %col] : memref<128x128xf64>
                %mul = arith.mulf %lhs, %rhs : f64
                %old = memref.load %arg2[%row, %col] : memref<128x128xf64>
                %sum = arith.addf %old, %mul : f64
                memref.store %sum, %arg2[%row, %col] : memref<128x128xf64>
              }
            }
          }
          codir.yield
        }
      }
    }
    memref.dealloc %c : memref<128x128xf64>
    memref.dealloc %b : memref<128x128xf64>
    memref.dealloc %a : memref<128x128xf64>
    func.return
  }
}

// CHECK-LABEL: func.func @matmul_inputs_use_committed_dep_block_shapes
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}sizes[%{{[^]]+}}]{{.*}}elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK-SAME: planOwnerDims = [0]
// CHECK-SAME: planPhysicalBlockShape = [64, 128]
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}sizes[%{{[^]]+}}]{{.*}}elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK-SAME: planOwnerDims = [1]
// CHECK-SAME: planPhysicalBlockShape = [128, 64]
// CHECK: arts.db_alloc{{.*}}<block>{{.*}}sizes[%{{[^,]+}}, %{{[^]]+}}]{{.*}}elementSizes[%{{[^,]+}}, %{{[^]]+}}]
// CHECK-SAME: planOwnerDims = [0, 1]
// CHECK-SAME: planPhysicalBlockShape = [16, 16]
// CHECK: arts.db_acquire[<in>] {{.*}}partitioning(<block>)
// CHECK: arts.db_acquire[<in>] {{.*}}partitioning(<block>)
// CHECK: arts.db_acquire[<inout>] {{.*}}partitioning(<block>)
// CHECK: arts.edt <task> <intranode>
// CHECK-SAME: depPattern = #arts.dep_pattern<matmul>
