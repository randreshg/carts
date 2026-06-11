// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps)' 2>&1 \
// RUN:   | %FileCheck %s

// A grouped writer range that crosses owner-map route boundaries cannot be
// emitted and left for a downstream pass to repair.

// CHECK: commits logicalWorkerSlice that groups distributed writer blocks across owner routes

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @owner_crossing_grouped_distributed_writer_rejected() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32
    %value = arith.constant 1.0 : f32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c4] {planOwnerDims = [0], planPhysicalBlockShape = [1, 4]} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %A = memref.cast %block : memref<?x?xf32> to memref<4x4xf32>

    sde.su_iterate (%c0) to (%c16) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      sde.cu_region <parallel> {
        "arts.db_access_plan"(%A) <{blockHi = [4], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [4]}> : (memref<4x4xf32>) -> ()
        %owner = arith.divui %i, %c4 : index
        %local = arith.remui %i, %c4 : index
        memref.store %value, %A[%owner, %local] : memref<4x4xf32>
      }
    } {physicalOwnerDims = [0], physicalBlockShape = [4], logicalWorkerSlice = [12]}
    return
  }
}
