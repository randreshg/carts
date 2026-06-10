// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=codir.codelet

module {
  func.func @rank_expanded_write_bridge_skips_copy_in() -> f64 {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %a = memref.alloc() : memref<8x4xf64>
    scf.for %base = %c0 to %c32 step %c16 {
      %block = arith.divui %base, %c4 : index
      %view = memref.subview %a[%block, 0] [1, 4] [1, 1] :
        memref<8x4xf64> to memref<1x4xf64, strided<[4, 1], offset: ?>>
      codir.codelet deps(%view : memref<1x4xf64, strided<[4, 1], offset: ?>>)
          params(%base, %block : index, index)
          attributes {array_layout = [{arrayId = 0 : i64, blockShape = [4], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"}],
                      completion_barrier,
                      dep_array_ids = [0],
                      dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<write>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [16],
                      pattern = #codir.pattern<uniform>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%arg0: memref<1x4xf64, strided<[4, 1], offset: ?>>, %arg1: index, %arg2: index):
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %inner_c32 = arith.constant 32 : index
        %v = arith.constant 1.000000e+00 : f64
        %end_raw = arith.addi %arg1, %inner_c16 : index
        %end = arith.minui %end_raw, %inner_c32 : index
        scf.for %i = %arg1 to %end step %inner_c4 {
          %tile_end_raw = arith.addi %i, %inner_c4 : index
          %tile_end = arith.minui %tile_end_raw, %inner_c32 : index
          scf.for %j = %i to %tile_end step %inner_c1 {
            %block_j = arith.divui %j, %inner_c4 : index
            %off_j = arith.remui %j, %inner_c4 : index
            %rel = arith.subi %block_j, %arg2 : index
            memref.store %v, %arg0[%rel, %off_j] : memref<1x4xf64, strided<[4, 1], offset: ?>>
          }
        }
        codir.yield
      }
    }
    %observed = memref.load %a[%c0, %c0] : memref<8x4xf64>
    memref.dealloc %a : memref<8x4xf64>
    func.return %observed : f64
  }

  func.func @rank_expanded_read_bridge_keeps_copy_in() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %v = arith.constant 1.000000e+00 : f64
    %a = memref.alloc() : memref<8x4xf64>
    memref.store %v, %a[%c0, %c0] : memref<8x4xf64>
    scf.for %base = %c0 to %c32 step %c16 {
      %block = arith.divui %base, %c4 : index
      %view = memref.subview %a[%block, 0] [1, 4] [1, 1] :
        memref<8x4xf64> to memref<1x4xf64, strided<[4, 1], offset: ?>>
      codir.codelet deps(%view : memref<1x4xf64, strided<[4, 1], offset: ?>>)
          params(%base, %block : index, index)
          attributes {array_layout = [{arrayId = 0 : i64, blockShape = [4], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "read"}],
                      dep_array_ids = [0],
                      dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<read>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [16],
                      pattern = #codir.pattern<uniform>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%arg0: memref<1x4xf64, strided<[4, 1], offset: ?>>, %arg1: index, %arg2: index):
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %inner_c32 = arith.constant 32 : index
        %end_raw = arith.addi %arg1, %inner_c16 : index
        %end = arith.minui %end_raw, %inner_c32 : index
        scf.for %i = %arg1 to %end step %inner_c4 {
          %tile_end_raw = arith.addi %i, %inner_c4 : index
          %tile_end = arith.minui %tile_end_raw, %inner_c32 : index
          scf.for %j = %i to %tile_end step %inner_c1 {
            %block_j = arith.divui %j, %inner_c4 : index
            %off_j = arith.remui %j, %inner_c4 : index
            %rel = arith.subi %block_j, %arg2 : index
            %unused = memref.load %arg0[%rel, %off_j] : memref<1x4xf64, strided<[4, 1], offset: ?>>
          }
        }
        codir.yield
      }
    }
    memref.dealloc %a : memref<8x4xf64>
    func.return
  }

  func.func @rank_expanded_multi_write_bridge_skips_copy_in() -> f64 {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %a = memref.alloc() : memref<8x4xf64>
    %b = memref.alloc() : memref<8x4xf64>
    %c = memref.alloc() : memref<8x4xf64>
    scf.for %base = %c0 to %c32 step %c16 {
      %block_a = arith.divui %base, %c4 : index
      %view_a = memref.subview %a[%block_a, 0] [1, 4] [1, 1] :
        memref<8x4xf64> to memref<1x4xf64, strided<[4, 1], offset: ?>>
      %block_b = arith.divui %base, %c4 : index
      %view_b = memref.subview %b[%block_b, 0] [1, 4] [1, 1] :
        memref<8x4xf64> to memref<1x4xf64, strided<[4, 1], offset: ?>>
      %block_c = arith.divui %base, %c4 : index
      %view_c = memref.subview %c[%block_c, 0] [1, 4] [1, 1] :
        memref<8x4xf64> to memref<1x4xf64, strided<[4, 1], offset: ?>>
      codir.codelet deps(%view_a, %view_b, %view_c : memref<1x4xf64, strided<[4, 1], offset: ?>>, memref<1x4xf64, strided<[4, 1], offset: ?>>, memref<1x4xf64, strided<[4, 1], offset: ?>>)
          params(%base, %block_a, %block_b, %block_c : index, index, index, index)
          attributes {array_layout = [{arrayId = 0 : i64, blockShape = [4], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"},
                                      {arrayId = 1 : i64, blockShape = [4], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"},
                                      {arrayId = 2 : i64, blockShape = [4], kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"}],
                      completion_barrier,
                      dep_array_ids = [0, 1, 2],
                      dep_collectives = [#codir.collective<none>, #codir.collective<none>, #codir.collective<none>],
                      dep_modes = [#codir.access_mode<write>, #codir.access_mode<write>, #codir.access_mode<write>],
                      dep_owner_dims = [[0], [0], [0]],
                      dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>],
                      distribution_kind = #codir.distribution_kind<blocked>,
                      iteration_topology = #codir.iteration_topology<owner_strip>,
                      logical_worker_slice = [16],
                      pattern = #codir.pattern<uniform>,
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%arg0: memref<1x4xf64, strided<[4, 1], offset: ?>>, %arg1: memref<1x4xf64, strided<[4, 1], offset: ?>>, %arg2: memref<1x4xf64, strided<[4, 1], offset: ?>>, %arg3: index, %arg4: index, %arg5: index, %arg6: index):
        %inner_c1 = arith.constant 1 : index
        %inner_c4 = arith.constant 4 : index
        %inner_c16 = arith.constant 16 : index
        %inner_c32 = arith.constant 32 : index
        %v0 = arith.constant 1.000000e+00 : f64
        %v1 = arith.constant 2.000000e+00 : f64
        %v2 = arith.constant 0.000000e+00 : f64
        %end_raw = arith.addi %arg3, %inner_c16 : index
        %end = arith.minui %end_raw, %inner_c32 : index
        scf.for %i = %arg3 to %end step %inner_c4 {
          %tile_end_raw = arith.addi %i, %inner_c4 : index
          %tile_end = arith.minui %tile_end_raw, %inner_c32 : index
          scf.for %j = %i to %tile_end step %inner_c1 {
            %block_j_a = arith.divui %j, %inner_c4 : index
            %off_j_a = arith.remui %j, %inner_c4 : index
            %rel_a = arith.subi %block_j_a, %arg4 : index
            memref.store %v0, %arg0[%rel_a, %off_j_a] : memref<1x4xf64, strided<[4, 1], offset: ?>>
            %block_j_b = arith.divui %j, %inner_c4 : index
            %off_j_b = arith.remui %j, %inner_c4 : index
            %rel_b = arith.subi %block_j_b, %arg5 : index
            memref.store %v1, %arg1[%rel_b, %off_j_b] : memref<1x4xf64, strided<[4, 1], offset: ?>>
            %block_j_c = arith.divui %j, %inner_c4 : index
            %off_j_c = arith.remui %j, %inner_c4 : index
            %rel_c = arith.subi %block_j_c, %arg6 : index
            memref.store %v2, %arg2[%rel_c, %off_j_c] : memref<1x4xf64, strided<[4, 1], offset: ?>>
          }
        }
        codir.yield
      }
    }
    %a0 = memref.load %a[%c0, %c0] : memref<8x4xf64>
    %b0 = memref.load %b[%c0, %c0] : memref<8x4xf64>
    %c0v = memref.load %c[%c0, %c0] : memref<8x4xf64>
    %ab = arith.addf %a0, %b0 : f64
    %sum = arith.addf %ab, %c0v : f64
    memref.dealloc %a : memref<8x4xf64>
    memref.dealloc %b : memref<8x4xf64>
    memref.dealloc %c : memref<8x4xf64>
    func.return %sum : f64
  }
}

// CHECK-LABEL: func.func @rank_expanded_write_bridge_skips_copy_in
// CHECK: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK-NOT: storageBridgeCopy
// CHECK: arts.db_acquire[<out>]{{.*}}partitioning(<block>){{.*}}preserve_access_mode
// CHECK: arts.edt <task>
// CHECK-SAME: depPattern = #arts.dep_pattern<uniform>
// CHECK-SAME: planLogicalWorkerSlice = [16]
// CHECK-SAME: planPhysicalBlockShape = [4]
// CHECK: arith.subi %{{.*}}, %arg{{[0-9]+}} : index
// CHECK: arts.db_ref %arg{{[0-9]+}}[%{{[A-Za-z0-9_]+}}] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}] : memref<?x?xf64>
// CHECK: storageBridgeCopy

// CHECK-LABEL: func.func @rank_expanded_read_bridge_keeps_copy_in
// CHECK: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: storageBridgeCopy
// CHECK: arts.barrier
// CHECK: arts.db_acquire[<in>]{{.*}}partitioning(<block>){{.*}}preserve_access_mode
// CHECK: arts.edt <task>
// CHECK-SAME: depPattern = #arts.dep_pattern<uniform>
// CHECK-SAME: planLogicalWorkerSlice = [16]
// CHECK-SAME: planPhysicalBlockShape = [4]

// CHECK-LABEL: func.func @rank_expanded_multi_write_bridge_skips_copy_in
// CHECK: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK: storage_bridge = #arts.storage_bridge<host_whole_to_compute_block>
// CHECK-NOT: storageBridgeCopy
// CHECK: arts.db_acquire[<out>]{{.*}}partitioning(<block>){{.*}}preserve_access_mode
// CHECK: arts.db_acquire[<out>]{{.*}}partitioning(<block>){{.*}}preserve_access_mode
// CHECK: arts.db_acquire[<out>]{{.*}}partitioning(<block>){{.*}}preserve_access_mode
// CHECK: arts.edt <task>
// CHECK-SAME: depPattern = #arts.dep_pattern<uniform>
// CHECK-SAME: planLogicalWorkerSlice = [16]
// CHECK-SAME: planPhysicalBlockShape = [4]
// CHECK: storageBridgeCopy
