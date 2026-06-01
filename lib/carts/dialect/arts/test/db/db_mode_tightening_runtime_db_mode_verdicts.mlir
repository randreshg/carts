// RUN: %carts-compile %s --pipeline post-db-refinement --start-from post-db-refinement --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

// DbModeTightening owns runtime dependency mode policy. It stamps explicit
// ARTS-level verdicts for ARTS-RT lowering to consume mechanically.

// CHECK-LABEL: func.func @runtime_db_mode_verdicts
// CHECK: arts.db_acquire[<{{out|inout}}>]{{.*}}runtime_db_mode = #arts.runtime_db_mode<rw>
// CHECK: arts.db_acquire[<inout>]{{.*}}runtime_db_mode = #arts.runtime_db_mode<ew>
// CHECK: arts.db_acquire[<in>]{{.*}}runtime_db_mode = #arts.runtime_db_mode<ro>

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @runtime_db_mode_verdicts() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %value = arith.constant 1.0 : f64

    %out_guid, %out_ptr = arts.db_alloc[<out>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %rw_guid, %rw_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c4] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %read_guid, %read_ptr = arts.db_alloc[<in>, <heap>, <read>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c4] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)

    %out_acq_guid, %out_acq_ptr = arts.db_acquire[<out>] (%out_guid : memref<?xi64>, %out_ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %rw_acq_guid, %rw_acq_ptr = arts.db_acquire[<inout>] (%rw_guid : memref<?xi64>, %rw_ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %read_acq_guid, %read_acq_ptr = arts.db_acquire[<in>] (%read_guid : memref<?xi64>, %read_ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <intranode> route(%route) (%out_acq_ptr, %rw_acq_ptr, %read_acq_ptr) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> attributes {planIterationTopology = #arts.plan_iteration_topology<owner_strip>, planLogicalWorkerSlice = [4]} {
    ^bb0(%out_dep: memref<?xmemref<?xf64>>, %rw_dep: memref<?xmemref<?xf64>>, %read_dep: memref<?xmemref<?xf64>>):
      %out_payload = arts.db_ref %out_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %rw_payload = arts.db_ref %rw_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %read_payload = arts.db_ref %read_dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %out_payload[%c0] : memref<?xf64>
      %old = memref.load %rw_payload[%c0] : memref<?xf64>
      %read = memref.load %read_payload[%c0] : memref<?xf64>
      %sum = arith.addf %old, %read : f64
      memref.store %sum, %rw_payload[%c0] : memref<?xf64>
      arts.yield
    }

    arts.db_release(%out_acq_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%rw_acq_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%read_acq_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
