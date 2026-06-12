// RUN: not %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @distributed_db_rejects_local_only() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
      {distributed, local_only}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK: cannot be both distributed and local_only
