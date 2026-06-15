// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-raw-access-covered)' 2>&1 | %FileCheck %s

// CHECK: touches a DB without a committed SDE access-window dependency

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @reject_copy_only_uncovered_target() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %src_guid, %src_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c16] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %src_block = arts.db_ref %src_ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %src = memref.cast %src_block : memref<?x?xf32> to memref<4x16xf32>

    %dst_guid, %dst_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c16] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %dst_block = arts.db_ref %dst_ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %dst = memref.cast %dst_block : memref<?x?xf32> to memref<4x16xf32>

    sde.su_iterate (%c0) to (%c16) step (%c16) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.cu_region <parallel> {
        "arts.db_access_window"(%src) <{blockHi = [4], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [16]}> : (memref<4x16xf32>) -> ()
        memref.copy %src, %dst : memref<4x16xf32> to memref<4x16xf32>
      }
    } {}
    return
  }
}
