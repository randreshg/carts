// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps)' 2>&1 | %FileCheck %s

// Typed SDE layout facts are authoritative at the boundary. If the access
// window for the same array identity carries a different owner rank, ARTS must
// reject the producer bug instead of ignoring the typed fact and deriving a
// replacement layout from access coordinates.

// CHECK: owner-dim count disagrees with committed SDE layout fact

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @reject_typed_layout_window_rank_mismatch() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %one = arith.constant 1.0 : f32
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2] elementType(f32) elementSizes[%c1, %c4, %c2] : (memref<?xi64>, memref<?xmemref<?x?x?xf32>>)
    %block0 = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?x?xf32>> -> memref<?x?x?xf32>
    %A = memref.cast %block0 : memref<?x?x?xf32> to memref<2x4x2xf32>

    sde.su_iterate (%c0, %c0) to (%c4, %c4) step (%c1, %c1) classification(<elementwise>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %A : memref<2x4x2xf32> array_id(0)
      sde.array_layout write array_id(0) owner [0, 1] block [4, 2] logical [4, 4]
      sde.cu_region <parallel> {
        "arts.db_access_window"(%A) <{arrayId = 0 : i64, blockHi = [2], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [4, 2]}> : (memref<2x4x2xf32>) -> ()
        %block = arith.divui %j, %c2 : index
        %col = arith.remui %j, %c2 : index
        memref.store %one, %A[%block, %i, %col] : memref<2x4x2xf32>
      }
    } {}
    return
  }
}
