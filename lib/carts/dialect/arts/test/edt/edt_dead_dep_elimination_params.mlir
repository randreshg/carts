// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @true_only_control_dep_with_params() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %value = arith.constant 1.0 : f64
    %true = arith.constant true

    %data_guid, %data_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c4] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %flag_guid, %flag_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(i1) elementSizes[%c1] {local_only} : (memref<?xi64>, memref<?xmemref<i1>>)

    %data_acq_guid, %data_acq_ptr = arts.db_acquire[<out>] (%data_guid : memref<?xi64>, %data_ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %flag_acq_guid, %flag_acq_ptr = arts.db_acquire[<out>] (%flag_guid : memref<?xi64>, %flag_ptr : memref<?xmemref<i1>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<i1>>)

    arts.edt <task> <intranode> route(%route) (%data_acq_ptr, %flag_acq_ptr) : memref<?xmemref<?xf64>>, memref<?xmemref<i1>> params(%c4 : index) {
    ^bb0(%data: memref<?xmemref<?xf64>>, %flag: memref<?xmemref<i1>>, %limit: index):
      %payload = arts.db_ref %data[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %flag_payload = arts.db_ref %flag[%c0] : memref<?xmemref<i1>> -> memref<i1>
      memref.store %true, %flag_payload[] : memref<i1>
      memref.store %value, %payload[%c0] : memref<?xf64>
      func.call @use(%limit) : (index) -> ()
      arts.yield
    }
    return
  }

  func.func private @use(index)
}

// CHECK-LABEL: func.func @true_only_control_dep_with_params
// CHECK-NOT: arts.db_acquire[<out>]{{.*}}memref<?xmemref<i1>>
// CHECK: arts.edt <task> <intranode> route(%{{.*}}) (%{{.*}}) : memref<?xmemref<?xf64>> params(%{{.*}} : index)
// CHECK-SAME: {
// CHECK-NOT: memref<?xmemref<i1>>
// CHECK-NOT: memref.store %{{.*}}, %{{.*}}[] : memref<i1>
// CHECK: func.call @use
// CHECK: return
