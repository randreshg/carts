// RUN: %carts-compile %s --pass-pipeline='builtin.module(writer-owner-route)' 2>&1 | %FileCheck %s

// Single-node runtimes do not need internode owner-route derivation. An EDT that
// writes multiple distributed DBs with different block grids (e.g. poisson-for
// init) must not fail WriterOwnerRoute at node_count=1.

// CHECK-NOT: owner route cannot be derived
// CHECK-NOT: signalPassFailure

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 64 : i64} {
  func.func @multi_writer_init_single_node_ok() {
    %c0 = arith.constant 0 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 0.0 : f64

    %guid_a, %ptr_a = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c2, %c2] {distributed} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %guid_b, %ptr_b = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c4, %c4] {distributed} : (memref<?xi64>, memref<?xmemref<?x?xf64>>)

    %write_a_guid, %write_a_ptr = arts.db_acquire[<out>] (%guid_a : memref<?xi64>, %ptr_a : memref<?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c2] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)
    %write_b_guid, %write_b_ptr = arts.db_acquire[<out>] (%guid_b : memref<?xi64>, %ptr_b : memref<?xmemref<?x?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c4] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?x?xf64>>)

    arts.edt <task> <intranode> route(%route) (%write_a_ptr, %write_b_ptr) : memref<?xmemref<?x?xf64>>, memref<?xmemref<?x?xf64>> {
    ^bb0(%out_a: memref<?xmemref<?x?xf64>>, %out_b: memref<?xmemref<?x?xf64>>):
      %payload_a = arts.db_ref %out_a[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      %payload_b = arts.db_ref %out_b[%c0] : memref<?xmemref<?x?xf64>> -> memref<?x?xf64>
      memref.store %value, %payload_a[%c0, %c0] : memref<?x?xf64>
      memref.store %value, %payload_b[%c0, %c0] : memref<?x?xf64>
      arts.db_release(%out_a) : memref<?xmemref<?x?xf64>>
      arts.db_release(%out_b) : memref<?xmemref<?x?xf64>>
      arts.yield
    }

    arts.db_release(%write_a_ptr) : memref<?xmemref<?x?xf64>>
    arts.db_release(%write_b_ptr) : memref<?xmemref<?x?xf64>>
    return
  }
}
