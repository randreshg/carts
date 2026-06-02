// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

// CHECK-LABEL: func.func @critical_path_weights_multi_db_fanout
// CHECK: arts.edt <task> <intranode> route({{.*}}) ({{.*}}, {{.*}}) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> attributes {critical_path_distance = 0 : i64}
// CHECK: arts.edt <task> <intranode> route({{.*}}) ({{.*}}, {{.*}}) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> attributes {critical_path_distance = 2 : i64}

module {
  func.func @critical_path_weights_multi_db_fanout() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid_a, %ptr_a = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %guid_b, %ptr_b = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %write_a_guid, %write_a_ptr = arts.db_acquire[<out>] (%guid_a : memref<?xi64>, %ptr_a : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %write_b_guid, %write_b_ptr = arts.db_acquire[<out>] (%guid_b : memref<?xi64>, %ptr_b : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%write_a_ptr, %write_b_ptr) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
    ^bb0(%write_a_dep: memref<?xmemref<?xf64>>, %write_b_dep: memref<?xmemref<?xf64>>):
      %write_a_view = arts.db_ref %write_a_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %write_b_view = arts.db_ref %write_b_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %write_a_view[%c0] : memref<?xf64>
      memref.store %value, %write_b_view[%c0] : memref<?xf64>
      arts.db_release(%write_a_dep) : memref<?xmemref<?xf64>>
      arts.db_release(%write_b_dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%write_a_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%write_b_ptr) : memref<?xmemref<?xf64>>

    %read_a_guid, %read_a_ptr = arts.db_acquire[<in>] (%guid_a : memref<?xi64>, %ptr_a : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %read_b_guid, %read_b_ptr = arts.db_acquire[<in>] (%guid_b : memref<?xi64>, %ptr_b : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%read_a_ptr, %read_b_ptr) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
    ^bb0(%read_a_dep: memref<?xmemref<?xf64>>, %read_b_dep: memref<?xmemref<?xf64>>):
      %read_a_view = arts.db_ref %read_a_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %read_b_view = arts.db_ref %read_b_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %loaded_a = memref.load %read_a_view[%c0] : memref<?xf64>
      %loaded_b = memref.load %read_b_view[%c0] : memref<?xf64>
      arts.db_release(%read_a_dep) : memref<?xmemref<?xf64>>
      arts.db_release(%read_b_dep) : memref<?xmemref<?xf64>>
      arts.yield
    }
    arts.db_release(%read_a_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%read_b_ptr) : memref<?xmemref<?xf64>>

    arts.db_free(%guid_a) : memref<?xi64>
    arts.db_free(%ptr_a) : memref<?xmemref<?xf64>>
    arts.db_free(%guid_b) : memref<?xi64>
    arts.db_free(%ptr_b) : memref<?xmemref<?xf64>>
    return
  }
}
