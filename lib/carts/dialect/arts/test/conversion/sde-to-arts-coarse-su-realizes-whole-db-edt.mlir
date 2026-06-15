// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.su_iterate --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_window

// CHECK-LABEL: func.func @coarse_su_realizes_whole_db_edt
// CHECK: %{{.*}}, %[[DEP:.*]] = arts.db_acquire[<inout>]
// CHECK-SAME: partitioning(<coarse>)
// CHECK: arts.edt <sync> <intranode> route
// CHECK-SAME: (%[[DEP]])
// CHECK: ^bb0(%[[TASK_DEP:.*]]: memref<?xmemref<?xf32>>,
// CHECK: arts.db_ref %[[TASK_DEP]]
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK: arts.barrier

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @coarse_su_realizes_whole_db_edt() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f32) elementSizes[%c8] : (memref<?xi64>, memref<?xmemref<?xf32>>)
    %payload = arts.db_ref %ptr[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
    %A = memref.cast %payload : memref<?xf32> to memref<8xf32>

    sde.su_iterate (%c0) to (%c8) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.cu_region <parallel> {
        %v = memref.load %A[%i] : memref<8xf32>
        memref.store %v, %A[%i] : memref<8xf32>
      }
    } {}
    return
  }
}
