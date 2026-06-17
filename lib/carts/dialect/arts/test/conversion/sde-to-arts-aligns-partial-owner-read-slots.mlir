// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not="commits conflicting dispatch loop dimensions" --implicit-check-not=sde.su_iterate --implicit-check-not=arts.db_access_window

// A 2-D writer may consume one-owner-dim inputs whose owner slots refer to
// different physical dimensions. The boundary aligns dependency slots by the
// committed physical owner dim instead of by local slot position.

// CHECK-LABEL: func.func @partial_owner_read_slots_align_to_writer_dims
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt
// CHECK: memref.store
// CHECK-LABEL: func.func @transposed_writer_rank_reduced_read_slot_aligns_by_loop
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt
// CHECK: memref.store

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @partial_owner_read_slots_align_to_writer_dims() {
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
        "arts.db_access_window"(%C) <{arrayId = 0 : i64, blockHi = [2, 2], blockLo = [0, 0], mode = #arts.mode<out>, ownerDimCount = 2 : i64, validExtents = [4, 4]}> : (memref<2x2x4x4xf32>) -> ()
        "arts.db_access_window"(%A) <{arrayId = 1 : i64, blockHi = [2], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [4, 8]}> : (memref<2x4x8xf32>) -> ()
        "arts.db_access_window"(%B) <{arrayId = 2 : i64, blockHi = [2], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [8, 4]}> : (memref<2x8x4xf32>) -> ()
        %ib = arith.divui %i, %c4 : index
        %jb = arith.divui %j, %c4 : index
        %il = arith.remui %i, %c4 : index
        %jl = arith.remui %j, %c4 : index
        %av = memref.load %A[%ib, %il, %j] : memref<2x4x8xf32>
        %bv = memref.load %B[%jb, %i, %jl] : memref<2x8x4xf32>
        %sum = arith.addf %av, %bv : f32
        %out = arith.addf %sum, %zero : f32
        memref.store %out, %C[%ib, %jb, %il, %jl] : memref<2x2x4x4xf32>
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [4, 4], muBlockCount = 4 : i64, role = "write"}, {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4, 8], muBlockCount = 2 : i64, role = "read"}, {arrayId = 2 : i64, kind = "block_parallel", ownerDims = [1], blockShape = [8, 4], muBlockCount = 2 : i64, role = "read"}], partialReduction, partialReductionDims = [2], partialReductionOwnerDims = [0, 1]}
    return
  }

  func.func @transposed_writer_rank_reduced_read_slot_aligns_by_loop() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %route = arith.constant -1 : i32
    %zero = arith.constant 0.0 : f32

    %cGuid, %cPtr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2, %c2] elementType(f32) elementSizes[%c1, %c1, %c4, %c4] : (memref<?x?xi64>, memref<?x?xmemref<?x?x?x?xf32>>)
    %vGuid, %vPtr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f32) elementSizes[%c1, %c4] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %cBlock = arts.db_ref %cPtr[%c0, %c0] : memref<?x?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
    %vBlock = arts.db_ref %vPtr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %C = memref.cast %cBlock : memref<?x?x?x?xf32> to memref<2x2x4x4xf32>
    %V = memref.cast %vBlock : memref<?x?xf32> to memref<2x4xf32>

    sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c1, %c1) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %C : memref<2x2x4x4xf32> array_id(10)
      sde.array_layout_root read %V : memref<2x4xf32> array_id(11)
      sde.cu_region <parallel> {
        "arts.db_access_window"(%C) <{arrayId = 10 : i64, blockHi = [2, 2], blockLo = [0, 0], mode = #arts.mode<out>, ownerDimCount = 2 : i64, validExtents = [4, 4]}> : (memref<2x2x4x4xf32>) -> ()
        "arts.db_access_window"(%V) <{arrayId = 11 : i64, blockHi = [2], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [4]}> : (memref<2x4xf32>) -> ()
        %ib = arith.divui %i, %c4 : index
        %jb = arith.divui %j, %c4 : index
        %il = arith.remui %i, %c4 : index
        %jl = arith.remui %j, %c4 : index
        %vv = memref.load %V[%ib, %il] : memref<2x4xf32>
        %out = arith.addf %vv, %zero : f32
        memref.store %out, %C[%jb, %ib, %jl, %il] : memref<2x2x4x4xf32>
      }
    } {arrayLayout = [{arrayId = 10 : i64, kind = "block_parallel", ownerDims = [0, 1], blockShape = [4, 4], muBlockCount = 4 : i64, role = "write"}, {arrayId = 11 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [4], muBlockCount = 2 : i64, role = "read"}]}
    return
  }
}
