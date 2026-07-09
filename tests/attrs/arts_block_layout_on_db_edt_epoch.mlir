// RUN: %carts-compile %s --pass-pipeline='builtin.module()' | %FileCheck %s

// Phase A: shared `#arts.block_layout` on db_acquire, edt, and epoch.

// CHECK-LABEL: func.func @arts_block_layout_on_db_edt_epoch
// CHECK: arts.db_acquire
// CHECK-SAME: block_layout = #arts.block_layout<
// CHECK-SAME: owner_dims = [0],
// CHECK-SAME: block_shape = [4],
// CHECK-SAME: distribution_kind = <block>>
// CHECK: arts.epoch
// CHECK-SAME: block_layout = #arts.block_layout<
// CHECK-SAME: owner_dims = [0],
// CHECK-SAME: block_shape = [4],
// CHECK-SAME: distribution_kind = <block>>
// CHECK: arts.edt
// CHECK-SAME: block_layout = #arts.block_layout<
// CHECK-SAME: owner_dims = [0],
// CHECK-SAME: block_shape = [4],
// CHECK-SAME: distribution_kind = <block>>

module {
  func.func @arts_block_layout_on_db_edt_epoch() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
        route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
        {block_layout = #arts.block_layout<
            owner_dims = [0],
            block_shape = [4],
            distribution_kind = <block>>}
        : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %acq_guid, %acq_ptr = arts.db_acquire[<in>](%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>)
        , indices[%c0] {block_layout = #arts.block_layout<
            owner_dims = [0],
            block_shape = [4],
            distribution_kind = <block>>}
        -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    %epoch = arts.epoch attributes {block_layout = #arts.block_layout<
        owner_dims = [0],
        block_shape = [4],
        distribution_kind = <block>>} {
      arts.edt <task> <intranode> route(%route) (%acq_ptr)
          : memref<?xmemref<?xf64>> attributes {block_layout = #arts.block_layout<
              owner_dims = [0],
              block_shape = [4],
              distribution_kind = <block>>} {
      ^bb0(%arg0: memref<?xmemref<?xf64>>):
        arts.yield
      }
      arts.yield
    } : i64
    return
  }
}
