// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-distributed-db-placement)' 2>&1 \
// RUN:   | %FileCheck %s

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @missing_owner_map() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1]
      {distributed, planOwnerDims = [0], planPhysicalBlockShape = [1]}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK: with distributed ownership requires owner_map_kind, owner_map_version, owner_map_dims, and owner_block_shape
