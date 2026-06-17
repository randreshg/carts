// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.su_iterate --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_window

// When ARTS splits a grouped writer CU to an owner-local DB-block range, nested
// source body loops must use the realized local span. Keeping the larger SDE
// tile step would write past the exclusive DB range and fold later DB blocks
// into the first acquired payload.

// CHECK-LABEL: func.func @split_writer_nested_body_uses_local_span
// CHECK: %[[C8:[A-Za-z0-9_]+]] = arith.constant 8 : index
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %[[C8]]
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt <task> <internode> route
// CHECK-SAME: ownerLocalWriterSplit
// CHECK: ^bb0
// CHECK: scf.for %{{.*}} = {{.*}} to {{.*}} step %{{.*}}
// CHECK: arith.addi %{{.*}}, %[[C8]]
// CHECK: arith.subi %{{.*}}, %{{.*}} : index
// CHECK: arts.db_ref %{{.*}}[%{{.*}}]
// CHECK: memref.store

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @split_writer_nested_body_uses_local_span() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c12 = arith.constant 12 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c4] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %A = memref.cast %block : memref<?x?xf32> to memref<4x4xf32>

    sde.su_iterate (%c0) to (%c16) step (%c12) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<4x4xf32> array_id(0)
      sde.cu_region <parallel> {
        "arts.db_access_window"(%A) <{blockHi = [4], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [4]}> : (memref<4x4xf32>) -> ()
        %tileEndRaw = arith.addi %i, %c12 : index
        %tileEnd = arith.minui %tileEndRaw, %c16 : index
        scf.for %x = %i to %tileEnd step %c1 {
          %blockIndex = arith.divui %x, %c4 : index
          %localIndex = arith.remui %x, %c4 : index
          memref.store %value, %A[%blockIndex, %localIndex] : memref<4x4xf32>
        }
      } {groupBlockCount = [3]}
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4], muBlockCount = 4 : i64, role = "write"}]}
    return
  }
}
