// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   | %FileCheck %s --implicit-check-not=codir.codelet

module {
  func.func @rank_expanded_direct_logical_grid_selects_lane_db() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %c32 = arith.constant 32 : index
    %a = memref.alloc() : memref<8x4xf64>
    scf.for %base = %c0 to %c32 step %c16 {
      codir.codelet deps(%a : memref<8x4xf64>)
          params(%base : index)
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
      ^bb0(%arg0: memref<8x4xf64>, %arg1: index):
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
            memref.store %v, %arg0[%block_j, %off_j] : memref<8x4xf64>
          }
        }
        codir.yield
      }
    }
    memref.dealloc %a : memref<8x4xf64>
    func.return
  }
}

// CHECK-LABEL: func.func @rank_expanded_direct_logical_grid_selects_lane_db
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: planPhysicalBlockShape = [1, 4]
// CHECK: %[[BASE:.+]] = arith.divui %{{.+}}, %c4{{(_[0-9]+)?}} : index
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: offsets[%[[BASE]]]
// CHECK: arts.edt <task>
// CHECK-SAME: planLogicalWorkerSlice = [16]
// CHECK-SAME: planPhysicalBlockShape = [4]
// CHECK: %[[GRID:.+]] = arith.divui %{{.+}}, %c4{{(_[0-9]+)?}} : index
// CHECK: %[[REL:.+]] = arith.subi %[[GRID]], %arg{{[0-9]+}} : index
// CHECK: %[[DB:.+]] = arts.db_ref %arg{{[0-9]+}}[%[[REL]]] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
// CHECK: memref.store %{{.+}}, %[[DB]][%{{.+}}, %{{.+}}] : memref<?x?xf64>
