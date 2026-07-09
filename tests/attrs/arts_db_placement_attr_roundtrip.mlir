// RUN: %carts-compile %s --pass-pipeline='builtin.module()' | %FileCheck %s

// Phase A round-trip for typed `#arts.db_placement` on `arts.db_alloc`.
// One enum replacing the six legacy placement unit flags.

// CHECK-LABEL: func.func @arts_db_placement_attr_roundtrip
// CHECK: db_placement = #arts.db_placement<distributed>

module {
  func.func @arts_db_placement_attr_roundtrip() {
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %route = arith.constant -1 : i32
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
        route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c1]
        {db_placement = #arts.db_placement<distributed>}
        : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
