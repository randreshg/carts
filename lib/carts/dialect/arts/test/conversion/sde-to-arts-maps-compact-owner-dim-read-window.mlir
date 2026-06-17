// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not='arrayLayout owner dimension is outside the access-window payload rank' --implicit-check-not='cannot map SDE access-window block coordinate'

// A compact read window can carry one payload slot for a non-leading physical
// owner dimension. ARTS must preserve the physical owner dim for routing while
// mapping owner slot 0 to payload slot 0 for access-window payload checks.

// CHECK-LABEL: func.func @compact_read_window_owner_dim_one
// CHECK: arts.db_acquire[<in>]
// CHECK: arts.edt
// CHECK: memref.load
// CHECK: memref.store

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @compact_read_window_owner_dim_one() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32
    %zero = arith.constant 0.0 : f32

    %read_guid, %read_ptr = arts.db_alloc[<in>, <heap>, <read>, <block>] route(%route : i32) sizes[%c2] elementType(f32) elementSizes[%c1, %c8] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %write_guid, %write_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2, %c2] elementType(f32) elementSizes[%c1, %c1, %c8, %c8] : (memref<?x?xi64>, memref<?x?xmemref<?x?x?x?xf32>>)
    %read_block = arts.db_ref %read_ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %write_block = arts.db_ref %write_ptr[%c0, %c0] : memref<?x?xmemref<?x?x?x?xf32>> -> memref<?x?x?x?xf32>
    %B = memref.cast %read_block : memref<?x?xf32> to memref<2x8xf32>
    %C = memref.cast %write_block : memref<?x?x?x?xf32> to memref<2x2x8x8xf32>

    sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c1, %c1) classification(<matmul>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root read %B : memref<2x8xf32> array_id(1)
      sde.array_layout_root write %C : memref<2x2x8x8xf32> array_id(0)
      sde.cu_region <parallel> {
        "arts.db_access_window"(%B) <{arrayId = 1 : i64, blockHi = [2], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [8]}> : (memref<2x8xf32>) -> ()
        "arts.db_access_window"(%C) <{arrayId = 0 : i64, blockHi = [2, 2], blockLo = [0, 0], mode = #arts.mode<out>, ownerDimCount = 2 : i64, validExtents = [8, 8]}> : (memref<2x2x8x8xf32>) -> ()
        %k = arith.remui %i, %c2 : index
        %jb = arith.divui %j, %c8 : index
        %jl = arith.remui %j, %c8 : index
        %value = memref.load %B[%k, %jl] : memref<2x8xf32>
        %ib = arith.divui %i, %c8 : index
        %il = arith.remui %i, %c8 : index
        %out = arith.addf %value, %zero : f32
        memref.store %out, %C[%ib, %jb, %il, %jl] : memref<2x2x8x8xf32>
      }
    } {arrayLayout = [
      {arrayId = 1 : i64, blockShape = [16, 8], kind = "block_parallel",
       muBlockCount = 2 : i64, ownerDims = [1], role = "read"},
      {arrayId = 0 : i64, blockShape = [8, 8], kind = "block_parallel",
       muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}]}
    return
  }
}
