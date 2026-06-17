// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.su_iterate --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_window

// Owner-local split lowering must not rewrite a nested contraction loop's unit
// step just because it shares the same source step value as the owner loops.

// CHECK-LABEL: func.func @nested_reduction_keeps_source_step
// CHECK: arts.edt
// CHECK: ^bb0
// CHECK: scf.for %{{[A-Za-z0-9_]+}} = {{.*}} step %[[STEP:[A-Za-z0-9_]+]] {
// CHECK: scf.for %{{[A-Za-z0-9_]+}} = {{.*}} step %[[STEP]] {
// CHECK: scf.for %{{[A-Za-z0-9_]+}} = {{.*}} step %[[STEP]] {
// CHECK: memref.load
// CHECK: memref.store

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @nested_reduction_keeps_source_step() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %route = arith.constant -1 : i32
    %zero = arith.constant 0.0 : f32

    %cGuid, %cPtr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2, %c2] elementType(f32) elementSizes[%c1, %c1, %c4, %c4] : (memref<?x?xi64>, memref<?x?xmemref<?x?x?x?xf32>>)
    %aGuid, %aPtr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f32) elementSizes[%c1, %c4, %c8] : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %bGuid, %bPtr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f32) elementSizes[%c1, %c8, %c4] : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %cBlock = arts.db_ref %cPtr[%c0, %c0] : memref<?x?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
    %aBlock = arts.db_ref %aPtr[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
    %bBlock = arts.db_ref %bPtr[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
    %C = memref.cast %cBlock : memref<?x?x?x?xf32> to memref<2x2x4x4xf32>
    %A = memref.cast %aBlock : memref<?x?x?xf32> to memref<2x4x8xf32>
    %B = memref.cast %bBlock : memref<?x?x?xf32> to memref<2x8x4xf32>

    sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c1, %c1) classification(<matmul>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %C : memref<2x2x4x4xf32> array_id(0)
      sde.array_layout_root read %A : memref<2x4x8xf32> array_id(1)
      sde.array_layout_root read %B : memref<2x8x4xf32> array_id(2)
      sde.cu_region <parallel> {
        "arts.db_access_window"(%C) <{arrayId = 0 : i64, blockHi = [2, 2], blockLo = [0, 0], mode = #arts.mode<inout>, ownerDimCount = 2 : i64, validExtents = [4, 4]}> : (memref<2x2x4x4xf32>) -> ()
        "arts.db_access_window"(%A) <{arrayId = 1 : i64, blockHi = [2], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [4, 8]}> : (memref<2x4x8xf32>) -> ()
        "arts.db_access_window"(%B) <{arrayId = 2 : i64, blockHi = [2], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [8, 4]}> : (memref<2x8x4xf32>) -> ()
        %ib = arith.divui %i, %c4 : index
        %jb = arith.divui %j, %c4 : index
        %il = arith.remui %i, %c4 : index
        %jl = arith.remui %j, %c4 : index
        memref.store %zero, %C[%ib, %jb, %il, %jl] : memref<2x2x4x4xf32>
        scf.for %k = %c0 to %c8 step %c1 {
          %av = memref.load %A[%ib, %il, %k] : memref<2x4x8xf32>
          %bv = memref.load %B[%jb, %k, %jl] : memref<2x8x4xf32>
          %prod = arith.mulf %av, %bv : f32
          %acc = memref.load %C[%ib, %jb, %il, %jl] : memref<2x2x4x4xf32>
          %sum = arith.addf %acc, %prod : f32
          memref.store %sum, %C[%ib, %jb, %il, %jl] : memref<2x2x4x4xf32>
        }
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [4, 4], muBlockCount = 4 : i64, role = "write"}, {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4, 8], muBlockCount = 2 : i64, role = "read"}, {arrayId = 2 : i64, kind = "block_parallel", ownerDims = [1], blockShape = [8, 4], muBlockCount = 2 : i64, role = "read"}], partialReduction, partialReductionDims = [2], partialReductionOwnerDims = [0, 1]}
    return
  }
}
