// RUN: not %carts-compile %s --pass-pipeline='builtin.module(distributed-launch-consistency)' 2>&1 | %FileCheck %s

// Localizing an EDT that also writes a distributed DB violates committed
// ownership. The codelet must be split or sequenced instead.

// CHECK: mixes a local-only distributed dependency with a distributed writer

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @local_only_with_distributed_writer_rejected() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c65536 = arith.constant 65536 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c4] {distributed} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    %coarse_guid, %coarse_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c65536] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %read_guid, %read_ptr = arts.db_acquire[<in>] (%coarse_guid : memref<?xi64>, %coarse_ptr : memref<?xmemref<?xf64>>) partitioning(<coarse>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)

    arts.edt <task> <internode> route(%route) (%write_ptr, %read_ptr) : memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>> {
    ^bb0(%out: memref<?xmemref<?xf64>>, %in: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %out[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%out) : memref<?xmemref<?xf64>>
      arts.db_release(%in) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.db_release(%write_ptr) : memref<?xmemref<?xf64>>
    arts.db_release(%read_ptr) : memref<?xmemref<?xf64>>
    return
  }
}
