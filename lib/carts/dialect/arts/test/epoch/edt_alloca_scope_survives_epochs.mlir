// RUN: %carts-compile %s --arts-config %S/../../../../../../tests/inputs/arts_1t.cfg --start-from=epochs --pipeline=epochs | %FileCheck %s

// CHECK-LABEL: func.func @edt_alloca_scope_survives_epochs
// CHECK: arts.epoch
// CHECK-NOT: memref.alloca() : memref<f32>
// CHECK: arts.edt
// CHECK: %[[TMP:.*]] = memref.alloca() : memref<f32>
// CHECK: memref.store %{{.*}}, %[[TMP]][]
// CHECK: func.call @touch(%[[TMP]])
// CHECK: memref.load %[[TMP]][]

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64
} {
  func.func @edt_alloca_scope_survives_epochs() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %zero = arith.constant 0.0 : f32

    %tmp = memref.alloca() : memref<f32>
    memref.store %zero, %tmp[] : memref<f32>
    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>]
      route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1]
      {perBlockReplicated} : (memref<?xi64>, memref<?xmemref<?xf32>>)

    scf.for %rep = %c0 to %c2 step %c1 {
      %acq_guid, %acq_ptr = arts.db_acquire[<inout>]
        (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf32>>)
        partitioning(<block>), indices[], offsets[%c0], sizes[%c1]
        -> (memref<?xi64>, memref<?xmemref<?xf32>>)
      arts.edt <task> <intranode> route(%route) (%acq_ptr)
          : memref<?xmemref<?xf32>> {
      ^bb0(%dep: memref<?xmemref<?xf32>>):
        %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
        %v = memref.load %payload[%c0] : memref<?xf32>
        func.call @touch(%tmp) : (memref<f32>) -> ()
        %old = memref.load %tmp[] : memref<f32>
        %sum = arith.addf %old, %v : f32
        memref.store %sum, %tmp[] : memref<f32>
        memref.store %sum, %payload[%c0] : memref<?xf32>
        arts.db_release(%dep) : memref<?xmemref<?xf32>>
        arts.yield
      }
      arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
    }
    return
  }

  func.func private @touch(memref<f32>)
}
