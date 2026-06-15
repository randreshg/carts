// RUN: %carts-compile %s --pass-pipeline='builtin.module(edt-split-for-mixed-deps,writer-owner-route,verify-arts-cdag)' | %FileCheck %s

// An explicit replicatedRead acquire is the ARTS-side availability proof for a
// read-only coarse DB consumed by an internode distributed writer. The pass must
// not infer this from read_only_after_init alone.

// CHECK-LABEL: func.func @explicit_replicated_read_with_distributed_writer_allowed
// CHECK: arts.db_acquire{{.*}}replicatedRead
// CHECK: %[[TOTAL_NODES:[A-Za-z0-9_]+]] = arts.runtime_query <total_nodes> -> i32
// CHECK: %[[ROUTE:[A-Za-z0-9_]+]] = arith.index_cast {{.*}} : index to i32
// CHECK: arts.edt <task> <internode> route(%[[ROUTE]]) (%{{.*}}, %{{.*}}) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>
// CHECK-NOT: mixes a local-only distributed dependency with a distributed writer

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @explicit_replicated_read_with_distributed_writer_allowed() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c1048576 = arith.constant 1048576 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c4] {distributed} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    %coarse_guid, %coarse_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1048576] {local_only, read_only_after_init} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %read_guid, %read_ptr = arts.db_acquire[<in>] (%coarse_guid : memref<?xi64>, %coarse_ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] {replicatedRead, runtime_db_mode = #arts.runtime_db_mode<ro>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%write_ptr, %read_ptr) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
    ^bb0(%out: memref<?xmemref<?xf64>>, %in: memref<?xmemref<?xf64>>):
      %out_payload = arts.db_ref %out[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %in_payload = arts.db_ref %in[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %loaded = memref.load %in_payload[%c0] : memref<?xf64>
      %sum = arith.addf %loaded, %value : f64
      memref.store %sum, %out_payload[%c0] : memref<?xf64>
      arts.db_release(%out) : memref<?xmemref<?xf64>>
      arts.db_release(%in) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%write_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%read_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
