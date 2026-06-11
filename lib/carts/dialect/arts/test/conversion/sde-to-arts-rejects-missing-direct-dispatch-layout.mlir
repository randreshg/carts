// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps)' 2>&1 \
// RUN:   | %FileCheck %s

// CHECK: requires committed physicalOwnerDims and physicalBlockShape for direct ARTS dispatch

func.func @missing_direct_dispatch_layout() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %c4 = arith.constant 4 : index
  %c256 = arith.constant 256 : index
  %route = arith.constant -1 : i32
  %cst = arith.constant 1.0 : f32
  %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c256] {planOwnerDims = [0], planPhysicalBlockShape = [1, 256]} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
  %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
  %A = memref.cast %block : memref<?x?xf32> to memref<4x256xf32>
  sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      "arts.db_access_plan"(%A) <{blockHi = [4], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [256]}> : (memref<4x256xf32>) -> ()
      memref.store %cst, %A[%c0, %i] : memref<4x256xf32>
    }
  }
  return
}
