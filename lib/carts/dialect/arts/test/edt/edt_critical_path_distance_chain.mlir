// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

// CHECK-LABEL: func.func @critical_path_distance_chain
// CHECK: arts.edt <task> <intranode> route({{.*}}) ({{.*}}) : memref<?xmemref<?xf64>> attributes {critical_path_distance = 0 : i64}
// CHECK: arts.edt <task> <intranode> route({{.*}}) ({{.*}}) : memref<?xmemref<?xf64>> attributes {critical_path_distance = 1 : i64}
// CHECK: arts.edt <task> <intranode> route({{.*}}) ({{.*}}) : memref<?xmemref<?xf64>> attributes {critical_path_distance = 2 : i64}

module {
  func.func @critical_path_distance_chain() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%write_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%write_dep: memref<?xmemref<?xf64>>):
      %write_view = arts.db_ref %write_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %write_view[%c0] : memref<?xf64>
      arts.db_release(%write_dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%write_ptr) : memref<?xmemref<?xf64>>

    %update_guid, %update_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%update_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%update_dep: memref<?xmemref<?xf64>>):
      %update_view = arts.db_ref %update_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %loaded = memref.load %update_view[%c0] : memref<?xf64>
      memref.store %loaded, %update_view[%c0] : memref<?xf64>
      arts.db_release(%update_dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%update_ptr) : memref<?xmemref<?xf64>>

    %read_guid, %read_ptr = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%read_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%read_dep: memref<?xmemref<?xf64>>):
      %read_view = arts.db_ref %read_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %result = memref.load %read_view[%c0] : memref<?xf64>
      memref.store %result, %read_view[%c0] : memref<?xf64>
      arts.db_release(%read_dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%read_ptr) : memref<?xmemref<?xf64>>

    arts.db_free(%guid) : memref<?xi64>
    arts.db_free(%ptr) : memref<?xmemref<?xf64>>
    return
  }
}
