// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.su_iterate --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_window

// A grouped writer whose DB owner slots are addressed in transposed loop order
// must be capped using the actual access loop mapping, not the layout owner-dim
// order. The owner-contiguous 2x2 DB grid is owner-local as 1x2 DB groups, not
// 2x1 groups.

// CHECK-LABEL: func.func @transposed_grouped_writer_owner_local
// CHECK: %[[C4:[A-Za-z0-9_]+]] = arith.constant 4 : index
// CHECK: %[[C8:[A-Za-z0-9_]+]] = arith.constant 8 : index
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[C4]]
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[C8]]
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK-SAME: sizes[%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}]
// CHECK: arts.edt <task> <internode> route
// CHECK-SAME: ownerLocalWriterSplit
// CHECK: ^bb0
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %{{.*}}
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %{{.*}}
// CHECK: arts.db_ref %{{.*}}[%{{.*}}, %{{.*}}]
// CHECK: memref.store

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @transposed_grouped_writer_owner_local() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2, %c2] elementType(f32) elementSizes[%c1, %c1, %c4, %c4] : (memref<?x?xi64>, memref<?x?xmemref<?x?x?x?xf32>>)
    %block = arts.db_ref %ptr[%c0, %c0] : memref<?x?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
    %A = memref.cast %block : memref<?x?x?x?xf32> to memref<2x2x4x4xf32>

    sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index):
      sde.cu_region <parallel> {
        "arts.db_access_window"(%A) <{blockHi = [2, 2], blockLo = [0, 0], mode = #arts.mode<out>, ownerDimCount = 2 : i64, validExtents = [4, 4]}> : (memref<2x2x4x4xf32>) -> ()
        %jBlock = arith.divui %j, %c4 : index
        %iBlock = arith.divui %i, %c4 : index
        %jLocal = arith.remui %j, %c4 : index
        %iLocal = arith.remui %i, %c4 : index
        memref.store %value, %A[%jBlock, %iBlock, %jLocal, %iLocal] : memref<2x2x4x4xf32>
      } {groupBlockCount = [2, 2]}
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [4, 4], muBlockCount = 4 : i64, role = "write"}]}
    return
  }
}
