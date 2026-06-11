// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps)' 2>&1 | %FileCheck %s

// The SDE-to-ARTS boundary may coalesce multiple access plans for one DB only
// after their committed window evidence is identical.

// CHECK: commits access-window evidence that conflicts with another window for the same DB

func.func @conflicting_access_plan_windows() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c256 = arith.constant 256 : index
  %route = arith.constant -1 : i32
  %value = arith.constant 1.000000e+00 : f32
  %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c256] {planOwnerDims = [0], planPhysicalBlockShape = [1, 256]} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
  %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
  %cast = memref.cast %block : memref<?x?xf32> to memref<4x256xf32>
  sde.su_iterate (%c0) to (%c256) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      "arts.db_access_plan"(%cast) <{blockHi = [4], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [256]}> : (memref<4x256xf32>) -> ()
      "arts.db_access_plan"(%cast) <{blockHi = [4], blockLo = [1], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [256]}> : (memref<4x256xf32>) -> ()
      memref.store %value, %cast[%c0, %i] : memref<4x256xf32>
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [256]}
  return
}
