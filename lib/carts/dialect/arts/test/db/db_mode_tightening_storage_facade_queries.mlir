// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

// CHECK-LABEL: func.func @storage_hints_from_ordered_acquire_facade
// CHECK: arts.db_alloc[<inout>, <heap>, <write>, <coarse>
// CHECK-SAME: local_only
// CHECK-SAME: read_only_after_init
// CHECK: arts.db_acquire[<out>]
// CHECK: arts.db_acquire[<in>]

module {
  func.func @storage_hints_from_ordered_acquire_facade() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %write_guid, %write_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%write_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%write_dep: memref<?xmemref<?xf64>>):
      %write_ref = arts.db_ref %write_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %write_ref[%c0] : memref<?xf64>
      arts.db_release(%write_dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%write_ptr) : memref<?xmemref<?xf64>>

    %read_guid, %read_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%read_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%read_dep: memref<?xmemref<?xf64>>):
      %read_ref = arts.db_ref %read_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %read = memref.load %read_ref[%c0] : memref<?xf64>
      arts.db_release(%read_dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%read_ptr) : memref<?xmemref<?xf64>>

    return
  }
}
