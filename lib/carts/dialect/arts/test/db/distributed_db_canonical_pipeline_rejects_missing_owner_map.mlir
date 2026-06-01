// RUN: not %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg --distributed-db 2>&1 \
// RUN:   | %FileCheck %s

// A DB cannot enter the distributed lowering path without an explicit owner
// map plan. The canonical --distributed-db pipeline must reject this before
// pre-lowering/ARTS-RT lowering can reserve GUIDs or create DBs.

module {
  func.func @missing_owner_map_rejected_before_arts_rt() {
    %route = arith.constant 0 : i32
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c1] {distributed, planOwnerDims = [0], planPhysicalBlockShape = [1]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}

// CHECK: with distributed ownership requires owner_map_kind, owner_map_version, owner_map_dims, and owner_block_shape
