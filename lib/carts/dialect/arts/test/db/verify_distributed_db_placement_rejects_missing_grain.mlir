// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-distributed-db-placement)' 2>&1 \
// RUN:   | %FileCheck %s

// The ARTS verifier fails closed when a distributed DB lacks or contradicts its
// memory-placement evidence: a complete owner map is not enough, the realized
// owner-scattered home must be readable and consistent too. The verifier walks
// every DB, so both inconsistent allocations below are reported.

module attributes {arts.runtime_total_nodes = 4 : i64, arts.runtime_total_workers = 256 : i64} {
  // A distributed DB with a complete owner map but no placement fact.
  // CHECK: error: {{.*}}distributed but lacks db_memory_placement evidence
  func.func @distributed_db_missing_placement_is_rejected() {
    %route = arith.constant 0 : i32
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16]
      {distributed, owner_block_shape = [16], owner_map_dims = [0],
       owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>,
       owner_map_version = 1 : i32, planOwnerDims = [0],
       planPhysicalBlockShape = [16]}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }

  // A distributed DB whose placement is node_local is an inconsistent home (a
  // node-local fallback for partitioned storage) and is also rejected.
  // CHECK: error: {{.*}}distributed but db_memory_placement is not owner_scattered
  func.func @distributed_db_node_local_placement_is_rejected() {
    %route = arith.constant 0 : i32
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16]
      {distributed,
       db_memory_placement = #arts.db_memory_placement<node_local>,
       owner_block_shape = [16], owner_map_dims = [0],
       owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>,
       owner_map_version = 1 : i32, planOwnerDims = [0],
       planPhysicalBlockShape = [16]}
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
