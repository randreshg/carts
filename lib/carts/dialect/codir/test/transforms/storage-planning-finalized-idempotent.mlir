// RUN: %carts-compile %s --pass-pipeline='builtin.module(storage-planning,verify-codir)' \
// RUN:   | %FileCheck %s

module {
  func.func @finalized_compute_block_plan_is_preserved() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %A = memref.alloc() : memref<8xf32>
    scf.for %i = %c0 to %c8 step %c4 {
      codir.codelet deps(%A : memref<8xf32>) params(%i : index)
          attributes {dep_collectives = [#codir.collective<none>],
                      dep_modes = [#codir.access_mode<readwrite>],
                      dep_owner_dims = [[0]],
                      dep_storage_views = [#codir.storage_view<compute_block>],
                      logical_worker_slice = [4],
                      tile_owner_dims = [0],
                      tile_shape = [4]} {
      ^bb0(%arg0: memref<8xf32>, %owner: index):
        %value = memref.load %arg0[%owner] : memref<8xf32>
        memref.store %value, %arg0[%owner] : memref<8xf32>
        codir.yield
      }
    }
    return
  }
}

// CHECK-LABEL: func.func @finalized_compute_block_plan_is_preserved
// CHECK: codir.codelet
// CHECK-SAME: dep_collectives = [#codir.collective<none>]
// CHECK-SAME: dep_owner_dims = [{{\[}}0]]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>]
