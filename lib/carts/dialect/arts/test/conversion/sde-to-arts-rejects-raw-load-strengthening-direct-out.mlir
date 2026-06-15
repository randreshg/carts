// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-raw-access-covered)' 2>&1 | %FileCheck %s

// CHECK: raw access strengthens a committed SDE access-window dependency

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @reject_raw_load_strengthening_direct_out() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c16] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %A = memref.cast %block : memref<?x?xf32> to memref<4x16xf32>

    sde.su_iterate (%c0) to (%c16) step (%c16) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.cu_region <parallel> {
        "arts.db_access_window"(%A) <{blockHi = [4], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [16]}> : (memref<4x16xf32>) -> ()
        // The committed window is `out`; the matching store keeps the window
        // well-formed, while the raw load strengthens it with an uncommitted
        // read dependency the boundary must reject.
        %value = memref.load %A[%c0, %c0] : memref<4x16xf32>
        memref.store %value, %A[%c0, %c0] : memref<4x16xf32>
      }
    } {}
    return
  }
}
