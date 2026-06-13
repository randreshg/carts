// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

// A supported block-grid DB is owner-routed ARTS storage.

// CHECK-LABEL: func.func @intranode_block_db_realizes_owner_route
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK-NOT: local_only
// CHECK: arts.runtime_query <total_nodes>
// CHECK: arts.edt <task> <internode> route(
// CHECK: return

// CHECK-LABEL: func.func @multi_writer_equivalent_owner_route
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK: arts.runtime_query <total_nodes>
// CHECK: arts.edt <task> <internode> route(
// CHECK: return

module {
  func.func @intranode_block_db_realizes_owner_route() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c8]
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<out>]
      (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>)
      partitioning(<block>), indices[], offsets[%c0], sizes[%c1]
      -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%route) (%acq_ptr)
        : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }

  func.func @multi_writer_equivalent_owner_route() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %value = arith.constant 1.0 : f64

    %a_guid, %a_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c8]
      : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %b_guid, %b_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c8]
      : (memref<?xi64>, memref<?xmemref<?xf64>>)

    scf.for %i = %c0 to %c8 step %c8 {
      %a_offset = arith.divui %i, %c8 : index
      %a_acq_guid, %a_acq_ptr = arts.db_acquire[<out>]
        (%a_guid : memref<?xi64>, %a_ptr : memref<?xmemref<?xf64>>)
        partitioning(<block>), indices[], offsets[%a_offset], sizes[%c1]
        -> (memref<?xi64>, memref<?xmemref<?xf64>>)
      %b_offset = arith.divui %i, %c8 : index
      %b_acq_guid, %b_acq_ptr = arts.db_acquire[<out>]
        (%b_guid : memref<?xi64>, %b_ptr : memref<?xmemref<?xf64>>)
        partitioning(<block>), indices[], offsets[%b_offset], sizes[%c1]
        -> (memref<?xi64>, memref<?xmemref<?xf64>>)

      arts.edt <task> <intranode> route(%route) (%a_acq_ptr, %b_acq_ptr)
          : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
      ^bb0(%a_dep: memref<?xmemref<?xf64>>, %b_dep: memref<?xmemref<?xf64>>):
        %a_payload = arts.db_ref %a_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        %b_payload = arts.db_ref %b_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        memref.store %value, %a_payload[%c0] : memref<?xf64>
        memref.store %value, %b_payload[%c0] : memref<?xf64>
        arts.db_release(%a_dep) : memref<?xmemref<?xf64>>
        arts.db_release(%b_dep) : memref<?xmemref<?xf64>>
        arts.yield
      }

      arts.db_release(%a_acq_ptr) : memref<?xmemref<?xf64>>
      arts.db_release(%b_acq_ptr) : memref<?xmemref<?xf64>>
    }
    return
  }
}
