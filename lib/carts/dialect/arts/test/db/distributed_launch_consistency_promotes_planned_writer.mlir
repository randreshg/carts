// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   | %FileCheck %s --implicit-check-not=distributed_reject_reason --implicit-check-not="arts.edt <task> <intranode>"

// A committed block-plan writer does not need to be pre-marked internode for
// DB owner-map realization. ARTS realizes the owner map, then promotes and
// routes the writer EDT to the DB owner.

// CHECK-LABEL: func.func @planned_writer_is_promoted_to_owner_route
// CHECK: arts.db_alloc
// CHECK-SAME: db_memory_placement = #arts.db_memory_placement<owner_scattered>
// CHECK-SAME: distributed
// CHECK-SAME: owner_map_kind = #arts.owner_map_kind<owner_dim_contiguous>
// CHECK: %[[TOTAL_NODES:.*]] = arts.runtime_query <total_nodes>
// CHECK: %[[TOTAL_NODES_INDEX:.*]] = arith.index_cast %[[TOTAL_NODES]] : i32 to index
// CHECK: %[[SCALED:.*]] = arith.muli {{.*}}, %[[TOTAL_NODES_INDEX]]
// CHECK: %[[ROUTE_INDEX:.*]] = arith.divui %[[SCALED]]
// CHECK: %[[ROUTE:.*]] = arith.index_cast %[[ROUTE_INDEX]] : index to i32
// CHECK: arts.edt <task> <internode> route(%[[ROUTE]])

module {
  func.func @planned_writer_is_promoted_to_owner_route() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c8 = arith.constant 8 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c8] {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [8], planOwnerDims = [0], planPhysicalBlockShape = [8]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c1], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> attributes {distribution_kind = #arts.distribution_kind<block>, planLogicalWorkerSlice = [8], planOwnerDims = [0], planPhysicalBlockShape = [8]} {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
