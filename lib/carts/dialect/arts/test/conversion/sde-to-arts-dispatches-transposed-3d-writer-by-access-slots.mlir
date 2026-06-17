// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.su_iterate --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_window --implicit-check-not="writes a distributed DB range that may span multiple owners"

// A transposed 3-D writer dispatches by the committed access-slot mapping, not
// by assuming owner slot N maps to source loop dim N. The owner loops are k, j,
// then i here, so the grouped writer acquires one owner-local DB block.

// CHECK-LABEL: func.func @transposed_3d_writer_dispatches_by_access_slots
// CHECK: %[[C4:[A-Za-z0-9_]+]] = arith.constant 4 : index
// CHECK: %[[C8:[A-Za-z0-9_]+]] = arith.constant 8 : index
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[C4]]
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[C4]]
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[C8]]
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt <task> <internode> route

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @transposed_3d_writer_dispatches_by_access_slots() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %c17 = arith.constant 17 : index
    %c33 = arith.constant 33 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4, %c4, %c4] elementType(f32) elementSizes[%c1, %c1, %c1, %c4, %c4, %c8] : (memref<?x?x?xi64>, memref<?x?x?xmemref<?x?x?x?x?x?xf32>>)
    %block = arts.db_ref %ptr[%c0, %c0, %c0] : memref<?x?x?xmemref<?x?x?x?x?x?xf32>> -> memref<?x?x?x?x?x?xf32>
    %A = memref.cast %block : memref<?x?x?x?x?x?xf32> to memref<4x4x4x4x4x8xf32>

    sde.su_iterate (%c1, %c1, %c1) to (%c33, %c17, %c17) step (%c1, %c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index, %k: index):
      sde.cu_region <parallel> {
        "arts.db_access_window"(%A) <{blockHi = [4, 4, 4], blockLo = [0, 0, 0], mode = #arts.mode<out>, ownerDimCount = 3 : i64, validExtents = [4, 4, 8]}> : (memref<4x4x4x4x4x8xf32>) -> ()
        %kb = arith.divui %k, %c4 : index
        %jb = arith.divui %j, %c4 : index
        %ib = arith.divui %i, %c8 : index
        %kl = arith.remui %k, %c4 : index
        %jl = arith.remui %j, %c4 : index
        %il = arith.remui %i, %c8 : index
        memref.store %value, %A[%kb, %jb, %ib, %kl, %jl, %il] : memref<4x4x4x4x4x8xf32>
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1, 2], blockShape = [4, 4, 8], muBlockCount = 64 : i64, role = "write"}]}
    return
  }
}
