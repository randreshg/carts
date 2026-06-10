// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,dep-storage-assignment,verify-codir)' \
// RUN:   | %FileCheck %s

module {
  func.func @partitioned_writer_uses_block_storage_when_cu_group_is_larger() {
    %c0 = arith.constant 0 : index
    %c30 = arith.constant 30 : index
    %c100 = arith.constant 100 : index
    %a = memref.alloc() : memref<100xf64>
    scf.for %base = %c0 to %c100 step %c30 {
      codir.codelet deps(%a : memref<100xf64>)
          params(%base : index)
          attributes {array_layout = [{arrayId = 0 : i64, blockShape = [10], kind = "block_parallel", muBlockCount = 10 : i64, ownerDims = [0], role = "write"}],
                      dep_array_ids = [0],
                      dep_modes = [#codir.access_mode<write>],
                      dep_storage_views = [#codir.storage_view<host_whole>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [30],
                      pattern = #codir.pattern<uniform>,
                      tile_owner_dims = [0],
                      tile_shape = [10]} {
      ^bb0(%arg0: memref<100xf64>, %arg1: index):
        %c1_inner = arith.constant 1 : index
        %c10_inner = arith.constant 10 : index
        %c100_inner = arith.constant 100 : index
        %v_inner = arith.constant 1.000000e+00 : f64
        %end_raw = arith.addi %arg1, %c10_inner : index
        %end = arith.minui %end_raw, %c100_inner : index
        scf.for %i = %arg1 to %end step %c1_inner {
          memref.store %v_inner, %arg0[%i] : memref<100xf64>
        }
        codir.yield
      }
    }
    memref.dealloc %a : memref<100xf64>
    func.return
  }

  func.func @scalar_no_layout_dep_keeps_empty_owner_dims() {
    %c0 = arith.constant 0 : index
    %flag = memref.alloc() : memref<i1>
    %a = memref.alloc() : memref<10xf64>
    codir.codelet deps(%flag, %a : memref<i1>, memref<10xf64>)
        params(%c0 : index)
        attributes {array_layout = [{arrayId = 0 : i64, blockShape = [10], kind = "block_parallel", muBlockCount = 1 : i64, ownerDims = [0], role = "write"}],
                    dep_array_ids = [-1, 0],
                    dep_modes = [#codir.access_mode<write>, #codir.access_mode<write>],
                    dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<host_whole>],
                    distribution_kind = #codir.distribution_kind<blocked>,
                    iteration_topology = #codir.iteration_topology<owner_strip>,
                    logical_worker_slice = [10],
                    pattern = #codir.pattern<uniform>,
                    tile_owner_dims = [0],
                    tile_shape = [10]} {
    ^bb0(%arg0: memref<i1>, %arg1: memref<10xf64>, %base: index):
      %true_inner = arith.constant true
      %v_inner = arith.constant 2.000000e+00 : f64
      memref.store %true_inner, %arg0[] : memref<i1>
      memref.store %v_inner, %arg1[%base] : memref<10xf64>
      codir.yield
    }
    memref.dealloc %a : memref<10xf64>
    memref.dealloc %flag : memref<i1>
    func.return
  }

  func.func @partitioned_rank_expanded_reader_uses_block_storage() {
    %c0 = arith.constant 0 : index
    %a = memref.alloc() : memref<4x10xf64>
    codir.codelet deps(%a : memref<4x10xf64>)
        params(%c0 : index)
        attributes {array_layout = [{arrayId = 0 : i64, blockShape = [40], kind = "block_parallel", muBlockCount = 1 : i64, ownerDims = [0], role = "read"}],
                    dep_array_ids = [0],
                    dep_modes = [#codir.access_mode<read>],
                    dep_storage_views = [#codir.storage_view<host_whole>],
                    distribution_kind = #codir.distribution_kind<blocked>,
                    iteration_topology = #codir.iteration_topology<owner_strip>,
                    logical_worker_slice = [30],
                    pattern = #codir.pattern<uniform>,
                    tile_owner_dims = [0],
                    tile_shape = [10]} {
    ^bb0(%arg0: memref<4x10xf64>, %base: index):
      %c10_inner = arith.constant 10 : index
      %block = arith.divui %base, %c10_inner : index
      %offset = arith.remui %base, %c10_inner : index
      memref.load %arg0[%block, %offset] : memref<4x10xf64>
      codir.yield
    }
    memref.dealloc %a : memref<4x10xf64>
    func.return
  }
}

// CHECK-LABEL: func.func @partitioned_writer_uses_block_storage_when_cu_group_is_larger
// CHECK: codir.codelet
// CHECK-SAME: dep_owner_dims = [{{\[}}0]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>]
// CHECK-SAME: logical_worker_slice = [30]
// CHECK-SAME: tile_shape = [10]

// CHECK-LABEL: func.func @scalar_no_layout_dep_keeps_empty_owner_dims
// CHECK: codir.codelet
// CHECK-SAME: dep_array_ids = [{{-1}}, 0]
// CHECK-SAME: dep_owner_dims = [{{\[\]}}, [0]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<host_whole>, #codir.storage_view<compute_block>]

// CHECK-LABEL: func.func @partitioned_rank_expanded_reader_uses_block_storage
// CHECK: codir.codelet
// CHECK-SAME: dep_owner_dims = [{{\[}}0]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>]
