// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-distributed-db-placement)' \
// RUN:   | %FileCheck %s

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  func.func @nonleading_physical_owner_dim_projects_to_rank1_db_owner_map() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c8, %c16]
      {distributed, owner_block_shape = [16], owner_map_dims = [0],
       owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>,
       owner_map_version = 1 : i32, planOwnerDims = [1],
       planPhysicalBlockShape = [8, 16]}
      : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    return
  }
}

// CHECK-LABEL: func.func @nonleading_physical_owner_dim_projects_to_rank1_db_owner_map
// CHECK: owner_block_shape = [16]
// CHECK-SAME: owner_map_dims = [0]
// CHECK-SAME: planOwnerDims = [1]
// CHECK-SAME: planPhysicalBlockShape = [8, 16]
