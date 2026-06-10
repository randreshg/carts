// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=codir.codelet

module {
  func.func @rank_expanded_grouped_subview_selects_lane_db() {
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
    memref.dealloc %a : memref<8x4xf64>
    func.return
  }
}

// CHECK-LABEL: func.func @rank_expanded_grouped_subview_selects_lane_db
// CHECK: arts.db_acquire[<out>]
// CHECK: arts.edt <task>
// CHECK: %{{[A-Za-z0-9_]+}} = arith.subi
// CHECK: %[[SELECTED:[A-Za-z0-9_]+]] = arts.db_ref %arg{{[0-9]+}}[%{{[A-Za-z0-9_]+}}] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
// CHECK: memref.store %{{[A-Za-z0-9_]+}}, %[[SELECTED]][%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}] : memref<?x?xf64>
