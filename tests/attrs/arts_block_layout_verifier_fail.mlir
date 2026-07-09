// RUN: not %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 | %FileCheck %s

// CHECK: 'arts.db_acquire' op block_layout owner_dims must be unique

module {
  func.func @arts_block_layout_rejects_duplicate_owner_dims() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
        route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
        : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %acq_guid, %acq_ptr = arts.db_acquire[<in>](%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>)
        , indices[%c0] {block_layout = #arts.block_layout<
            owner_dims = [0, 0],
            block_shape = [4, 4],
            distribution_kind = <block>>}
        -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
