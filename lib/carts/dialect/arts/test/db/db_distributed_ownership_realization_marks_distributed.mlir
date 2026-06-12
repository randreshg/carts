// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_multinode.cfg | %FileCheck %s

// A block-grid DB used by an internode task is partitioned SDE structure. ARTS
// must prove owner routes from the concrete DB grid and mark distributed
// ownership. The block grain stays readable as the DB shape.

// CHECK-LABEL: func.func @block_grid_db_realizes_owner_route
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK: arts.edt <task> <internode>

// CHECK-LABEL: func.func @rank_short_db_grid_realizes_leading_owner_route
// CHECK: arts.db_alloc
// CHECK-SAME: <block>
// CHECK-SAME: distributed
// CHECK: arts.edt <task> <internode>

module {
  func.func @block_grid_db_realizes_owner_route() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {distribution_kind = #arts.distribution_kind<block>} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> attributes {distribution_kind = #arts.distribution_kind<block>} {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }

  func.func @rank_short_db_grid_realizes_leading_owner_route() {
    %c0 = arith.constant 0 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<in>, <heap>, <read>, <block>] route(%route : i32) sizes[%c4, %c2] elementType(f64) elementSizes[%c8] {distribution_kind = #arts.distribution_kind<block_cyclic>} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %acq_guid, %acq_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[%c0, %c0] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%acq_ptr) : memref<?xmemref<?xf64>> attributes {distribution_kind = #arts.distribution_kind<block_cyclic>} {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %dep[] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %value = memref.load %payload[%c0] : memref<?xf64>
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
