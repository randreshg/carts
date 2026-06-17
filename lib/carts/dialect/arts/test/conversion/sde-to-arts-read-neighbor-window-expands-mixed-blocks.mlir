// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.su_iterate --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_window --implicit-check-not='cannot map SDE access-window block coordinate'

// A read window with signed neighbor offsets must acquire the DB blocks touched
// by those shifted element accesses, not only the block containing the dispatch
// base. This matters when the writer and reader DB block sizes differ.

// CHECK-LABEL: func.func @transposed_3d_read_neighbor_window_mixed_blocks
// CHECK: arith.constant -1 : index
// CHECK: arith.addi
// CHECK: arith.maxsi
// CHECK: arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.db_ref
// CHECK: memref.load
// CHECK: memref.store

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @transposed_3d_read_neighbor_window_mixed_blocks() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %c4 = arith.constant 4 : index
    %c72 = arith.constant 72 : index
    %c96 = arith.constant 96 : index
    %c144 = arith.constant 144 : index
    %c192 = arith.constant 192 : index
    %c286 = arith.constant 286 : index
    %c382 = arith.constant 382 : index
    %route = arith.constant -1 : i32
    %zero = arith.constant 0.0 : f64

    %read_guid, %read_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c2, %c2, %c2] elementType(f64) elementSizes[%c1, %c1, %c1, %c144, %c144, %c192] : (memref<?x?x?xi64>, memref<?x?x?xmemref<?x?x?x?x?x?xf64>>)
    %write_guid, %write_ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4, %c4, %c4] elementType(f64) elementSizes[%c1, %c1, %c1, %c72, %c72, %c96] : (memref<?x?x?xi64>, memref<?x?x?xmemref<?x?x?x?x?x?xf64>>)
    %read_block = arts.db_ref %read_ptr[%c0, %c0, %c0] : memref<?x?x?xmemref<?x?x?x?x?x?xf64>> -> memref<?x?x?x?x?x?xf64>
    %write_block = arts.db_ref %write_ptr[%c0, %c0, %c0] : memref<?x?x?xmemref<?x?x?x?x?x?xf64>> -> memref<?x?x?x?x?x?xf64>
    %R = memref.cast %read_block : memref<?x?x?x?x?x?xf64> to memref<2x2x2x144x144x192xf64>
    %W = memref.cast %write_block : memref<?x?x?x?x?x?xf64> to memref<4x4x4x72x72x96xf64>

    sde.su_iterate (%c2, %c2, %c2) to (%c286, %c286, %c382) step (%c1, %c1, %c1) classification(<stencil>) {
    ^bb0(%i: index, %j: index, %k: index):
      sde.array_layout_root read %R : memref<2x2x2x144x144x192xf64> array_id(0)
      sde.array_layout_root write %W : memref<4x4x4x72x72x96xf64> array_id(1)
      sde.cu_region <parallel> {
        "arts.db_access_window"(%R) <{blockHi = [2, 2, 2], blockLo = [0, 0, 0], mode = #arts.mode<in>, ownerDimCount = 3 : i64, validExtents = [144, 144, 192]}> : (memref<2x2x2x144x144x192xf64>) -> ()
        "arts.db_access_window"(%W) <{blockHi = [4, 4, 4], blockLo = [0, 0, 0], mode = #arts.mode<out>, ownerDimCount = 3 : i64, validExtents = [72, 72, 96]}> : (memref<4x4x4x72x72x96xf64>) -> ()
        %im1 = arith.subi %i, %c1 : index
        %ip1 = arith.addi %i, %c1 : index
        %kp1 = arith.addi %k, %c1 : index
        %rib0 = arith.divui %im1, %c144 : index
        %ril0 = arith.remui %im1, %c144 : index
        %rib1 = arith.divui %ip1, %c144 : index
        %ril1 = arith.remui %ip1, %c144 : index
        %rjb = arith.divui %j, %c144 : index
        %rjl = arith.remui %j, %c144 : index
        %rkb = arith.divui %kp1, %c192 : index
        %rkl = arith.remui %kp1, %c192 : index
        %left = memref.load %R[%rib0, %rjb, %rkb, %ril0, %rjl, %rkl] : memref<2x2x2x144x144x192xf64>
        %right = memref.load %R[%rib1, %rjb, %rkb, %ril1, %rjl, %rkl] : memref<2x2x2x144x144x192xf64>
        %sum = arith.addf %left, %right : f64
        %wib = arith.divui %i, %c72 : index
        %wil = arith.remui %i, %c72 : index
        %wjb = arith.divui %j, %c72 : index
        %wjl = arith.remui %j, %c72 : index
        %wkb = arith.divui %k, %c96 : index
        %wkl = arith.remui %k, %c96 : index
        %out = arith.addf %sum, %zero : f64
        memref.store %out, %W[%wib, %wjb, %wkb, %wil, %wjl, %wkl] : memref<4x4x4x72x72x96xf64>
      }
    } {arrayLayout = [
      {arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0, 1, 2],
       blockShape = [144, 144, 192], muBlockCount = 8 : i64, role = "read"},
      {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [0, 1, 2],
       blockShape = [72, 72, 96], muBlockCount = 64 : i64, role = "write"}],
       accessMinOffsets = [-1, 0, 0], accessMaxOffsets = [1, 0, 1],
       ownerDims = [0, 1, 2], spatialDims = [0, 1, 2], writeFootprint = [0, 0, 0]}
    return
  }
}
