// RUN: not %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 | %FileCheck %s

// Typed block_layout is authoritative once present. Legacy stencil/distribution
// attrs may coexist during migration, but they must not disagree.

// CHECK: block_layout owner_dims must match legacy stencil_owner_dims

module {
  func.func @arts_block_layout_rejects_legacy_owner_dim_mismatch() {
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
        route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
        {block_layout = #arts.block_layout<
            owner_dims = [0],
            block_shape = [4],
            distribution_kind = <block>>,
         stencil_owner_dims = [1]}
        : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
