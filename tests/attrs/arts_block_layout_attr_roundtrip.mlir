// RUN: %carts-compile %s --pass-pipeline='builtin.module()' | %FileCheck %s

// Phase A round-trip for the typed `#arts.block_layout` attr on `arts.db_alloc`.
// Pins owner_dims, block_shape, optional halo_reach, and distribution_kind.

// CHECK-LABEL: func.func @arts_block_layout_attr_roundtrip
// CHECK: block_layout = #arts.block_layout<
// CHECK-SAME: owner_dims = [0, 1],
// CHECK-SAME: block_shape = [8, 16],
// CHECK-SAME: halo_reach = [1, 1],
// CHECK-SAME: distribution_kind = <block>>

module {
  func.func @arts_block_layout_attr_roundtrip() {
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
        route(%route : i32) sizes[%c8, %c16] elementType(f64) elementSizes[%c1, %c1]
        {block_layout = #arts.block_layout<
            owner_dims = [0, 1],
            block_shape = [8, 16],
            halo_reach = [1, 1],
            distribution_kind = <block>>}
        : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
