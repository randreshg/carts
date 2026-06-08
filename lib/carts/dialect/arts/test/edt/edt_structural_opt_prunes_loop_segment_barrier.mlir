// RUN: %carts-compile %s --pipeline edt-transforms --start-from edt-transforms --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @loop_segment_barrier_uses_db_frontier() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c1] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    scf.for %i = %c0 to %c2 step %c1 {
      %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%i], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
      arts.edt <task> <intranode> route(%route) (%write_ptr) : memref<?xmemref<?xf64>> {
      ^bb0(%dep: memref<?xmemref<?xf64>>):
        %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        memref.store %value, %payload[%c0] : memref<?xf64>
        arts.db_release(%dep) : memref<?xmemref<?xf64>>
        arts.yield
      }
    }

    arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}

    scf.for %i = %c0 to %c2 step %c1 {
      %read_guid, %read_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%i], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
      arts.edt <task> <intranode> route(%route) (%read_ptr) : memref<?xmemref<?xf64>> {
      ^bb0(%dep: memref<?xmemref<?xf64>>):
        %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
        %loaded = memref.load %payload[%c0] : memref<?xf64>
        func.call @use(%loaded) : (f64) -> ()
        arts.db_release(%dep) : memref<?xmemref<?xf64>>
        arts.yield
      }
    }

    arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
    return
  }

  func.func @host_side_effect_keeps_barrier() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%write_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      memref.store %value, %payload[%c0] : memref<?xf64>
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
    func.call @host_effect() : () -> ()

    %read_guid, %read_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%c0], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
    arts.edt <task> <intranode> route(%route) (%read_ptr) : memref<?xmemref<?xf64>> {
    ^bb0(%dep: memref<?xmemref<?xf64>>):
      %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
      %loaded = memref.load %payload[%c0] : memref<?xf64>
      func.call @use(%loaded) : (f64) -> ()
      arts.db_release(%dep) : memref<?xmemref<?xf64>>
      arts.yield
    }

    arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
    return
  }

  func.func @loop_latch_barrier_uses_next_iteration_prefix() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %value = arith.constant 1.0 : f64

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f64) elementSizes[%c1] {local_only} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    scf.for %step = %c0 to %c2 step %c1 {
      scf.for %i = %c0 to %c2 step %c1 {
        %write_guid, %write_ptr = arts.db_acquire[<out>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%i], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
        arts.edt <task> <intranode> route(%route) (%write_ptr) : memref<?xmemref<?xf64>> {
        ^bb0(%dep: memref<?xmemref<?xf64>>):
          %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
          memref.store %value, %payload[%c0] : memref<?xf64>
          arts.db_release(%dep) : memref<?xmemref<?xf64>>
          arts.yield
        }
      }

      arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}

      scf.for %i = %c0 to %c2 step %c1 {
        %read_guid, %read_ptr = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<?xf64>>) partitioning(<block>), indices[], offsets[%i], sizes[%c1] -> (memref<?xi64>, memref<?xmemref<?xf64>>)
        arts.edt <task> <intranode> route(%route) (%read_ptr) : memref<?xmemref<?xf64>> {
        ^bb0(%dep: memref<?xmemref<?xf64>>):
          %payload = arts.db_ref %dep[%c0] : memref<?xmemref<?xf64>> -> memref<?xf64>
          %loaded = memref.load %payload[%c0] : memref<?xf64>
          func.call @use(%loaded) : (f64) -> ()
          arts.db_release(%dep) : memref<?xmemref<?xf64>>
          arts.yield
        }
      }

      arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
    }

    arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
    return
  }

  func.func private @use(f64)
  func.func private @host_effect()
}

// CHECK-LABEL: func.func @loop_segment_barrier_uses_db_frontier
// CHECK: scf.for
// CHECK: arts.edt
// CHECK-NOT: arts.barrier
// CHECK: scf.for
// CHECK: arts.edt
// CHECK: arts.barrier
// CHECK: return

// CHECK-LABEL: func.func @host_side_effect_keeps_barrier
// CHECK: arts.edt
// CHECK: arts.barrier
// CHECK-NEXT: call @host_effect

// CHECK-LABEL: func.func @loop_latch_barrier_uses_next_iteration_prefix
// CHECK: scf.for
// CHECK: scf.for
// CHECK: arts.edt
// CHECK-NOT: arts.barrier
// CHECK: scf.for
// CHECK: arts.edt
// CHECK-NOT: arts.barrier
// CHECK: arts.barrier
// CHECK: return
