// RUN: not %carts-compile %s --pass-pipeline='builtin.module(edt-split-for-mixed-deps,writer-owner-route)' 2>&1 | %FileCheck %s

// A distributed writer whose acquire range spans the whole 4-block grid cannot
// be routed to a single owner under the derived owner-contiguous route at two
// nodes (block 0 -> node 0, block 3 -> node 1). WriterOwnerRoute must fail
// closed: SDE-to-ARTS owes an owner-local block-range split before launch.

// CHECK: writes a distributed DB range that may span multiple owners
// CHECK: SDE-to-ARTS must split writer codelets into owner-local block ranges before distributed launch

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @distributed_writer_spans_multiple_owners_rejected() {
    %c0 = arith.constant 0 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c4] {distributed} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c4] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%write_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%out: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %out[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%out) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%write_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
