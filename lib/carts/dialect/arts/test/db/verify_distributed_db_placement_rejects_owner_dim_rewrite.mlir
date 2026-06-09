// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 \
// RUN:   | %FileCheck %s

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @owner_map_dims_must_preserve_plan_owner_dims() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4, %c4] elementType(f64) elementSizes[%c1]
      {distributed, owner_block_shape = [1], owner_map_dims = [1],
       owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>,
       owner_map_version = 1 : i32, planOwnerDims = [0],
       planPhysicalBlockShape = [1]}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK: owner_map_dims must preserve planOwnerDims for distributed ownership
