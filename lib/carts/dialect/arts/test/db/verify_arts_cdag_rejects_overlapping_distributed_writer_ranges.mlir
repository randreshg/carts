// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 | %FileCheck %s

// CHECK: is a second concurrent writer of an overlapping distributed DB block range within one epoch

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @overlapping_distributed_writer_ranges_rejected() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c4] {distributed} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %first_guid, %first_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c2] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    %second_guid, %second_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c1], sizes[%c1] {runtime_db_mode = #arts.runtime_db_mode<ew>} -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.epoch {
      arts.edt <task> <internode> route(%route) (%first_ptr) : memref<?xmemref<?xf64>> {
      ^bb0(%out: memref<?xmemref<?xf64>>):
        %payload = arts.db_ref %out[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        memref.store %value, %payload[%c0] : memref<?xf64>
        arts.db_release(%out) : memref<?xmemref<?xf64>>
        arts.yield
      }
      arts.edt <task> <internode> route(%route) (%second_ptr) : memref<?xmemref<?xf64>> {
      ^bb0(%out: memref<?xmemref<?xf64>>):
        %payload = arts.db_ref %out[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        memref.store %value, %payload[%c0] : memref<?xf64>
        arts.db_release(%out) : memref<?xmemref<?xf64>>
        arts.yield
      }
      arts.yield
    } : i64

    arts.db_release(%first_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%second_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
