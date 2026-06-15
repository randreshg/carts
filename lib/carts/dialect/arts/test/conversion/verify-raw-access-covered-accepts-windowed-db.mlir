// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-raw-access-covered)' 2>&1 | %FileCheck %s

// Coverage companion to verify-raw-access-covered-rejects-unwindowed-db.mlir:
// every raw DB-backed access in the sde.su_iterate body is already covered by a
// committed SDE access-window dependency (the `in` DB by an `in` window, the
// `out` DB by an `out` window). The standalone gate authors no ARTS structure
// and selects no policy; it accepts the module unchanged.

// CHECK-LABEL: func.func @accept_windowed_db_access
// CHECK: sde.su_iterate
// CHECK: "arts.db_access_window"
// CHECK-SAME: mode = #arts.mode<in>
// CHECK: "arts.db_access_window"
// CHECK-SAME: mode = #arts.mode<out>

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @accept_windowed_db_access() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %c64 = arith.constant 64 : index
    %route = arith.constant -1 : i32

    %out_guid, %out_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c16] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %out_block = arts.db_ref %out_ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %out = memref.cast %out_block : memref<?x?xf32> to memref<4x16xf32>

    %in_guid, %in_ptr = arts.db_alloc[<inout>, <heap>, <write>, <coarse>] route(%route : i32) sizes[%c1] elementType(f32) elementSizes[%c64, %c16] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %in_block = arts.db_ref %in_ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %in = memref.cast %in_block : memref<?x?xf32> to memref<64x16xf32>

    sde.su_iterate (%c0) to (%c64) step (%c16) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.cu_region <parallel> {
        "arts.db_access_window"(%in) <{blockHi = [64], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [16]}> : (memref<64x16xf32>) -> ()
        "arts.db_access_window"(%out) <{blockHi = [4], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [16]}> : (memref<4x16xf32>) -> ()
        %block = arith.divui %i, %c16 : index
        scf.for %j = %c0 to %c16 step %c1 {
          %value = memref.load %in[%i, %j] : memref<64x16xf32>
          memref.store %value, %out[%block, %j] : memref<4x16xf32>
        }
      }
    } {}
    return
  }
}
