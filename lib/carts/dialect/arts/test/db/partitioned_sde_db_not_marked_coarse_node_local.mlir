// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg | %FileCheck %s --implicit-check-not="db_memory_placement = #arts.db_memory_placement<node_local>" --implicit-check-not=distributed_reject_reason

// A partitioned (block-planned) MU consumed internode must be realized as an
// owner-scattered distributed DB. ARTS must not demote it to a node-local /
// coarse home or stamp a distributed reject reason: that would be the silent
// coarse fallback the memory model forbids once SDE committed partitioned grain.

// Attributes print in alphabetical order, so the checks follow that order.
// CHECK-LABEL: func.func @partitioned_mu_stays_owner_scattered
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: db_memory_placement = #arts.db_memory_placement<owner_scattered>
// CHECK-SAME: distributed
// CHECK: arts.edt <task> <internode>

module {
  func.func @partitioned_mu_stays_owner_scattered() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2, %c4] elementType(f64) elementSizes[%c8, %c16] {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [8, 16], planOwnerDims = [0, 1], planPhysicalBlockShape = [8, 16]} : (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?x?xi64>, %ptr : memref<?x?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%c1, %c2], sizes[%c1, %c1] -> (memref<?x?xi64>, memref<?x?xmemref<?x?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?x?xmemref<?x?xf64>> attributes {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [8, 16], planOwnerDims = [0, 1], planPhysicalBlockShape = [8, 16]} {
    ^bb0(%dep: memref<?x?xmemref<?x?xf64>>):
      %payload = arts.db_ref %dep[%c0, %c0] : memref<?x?xmemref<?x?xf64>> -> memref<?x?xf64>
      memref.store %value, %payload[%c0, %c0] : memref<?x?xf64>
      arts.db_release(%dep) : memref<?x?xmemref<?x?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?x?xmemref<?x?xf64>>
    return
  }
}
