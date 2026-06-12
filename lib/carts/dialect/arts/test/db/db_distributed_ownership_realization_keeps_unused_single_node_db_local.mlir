// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

// Single-node runtime does not make an otherwise local block DB distributed.
// ARTS needs positive distributed use or distribution facts before it marks
// distributed ownership.

// CHECK-LABEL: func.func @single_node_local_block_db_stays_local
// CHECK: arts.db_alloc
// CHECK-NOT: distributed
// CHECK: arts.edt <task> <intranode>
// CHECK: return

module {
  func.func @single_node_local_block_db_stays_local() {
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
}
