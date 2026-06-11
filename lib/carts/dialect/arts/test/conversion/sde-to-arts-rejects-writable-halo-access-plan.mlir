// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps)' 2>&1 | %FileCheck %s

// A halo access window is a read-side movement fact. If SDE has an in-place
// update with halo reads, it must split read-halo and write windows before ARTS.

// CHECK: commits a writable halo dependency; SDE must split the read halo and write access before ARTS realization

func.func @writable_halo_access_plan_rejected() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c16 = arith.constant 16 : index
  %route = arith.constant -1 : i32
  %value = arith.constant 1.000000e+00 : f32

  %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c16] {planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf32>>)
  %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?xf32>> -> memref<?xf32>
  %cast = memref.cast %block : memref<?xf32> to memref<64xf32>

  sde.su_iterate (%c0) to (%c16) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      "arts.db_access_plan"(%cast) <{blockHi = [4], blockLo = [0], haloShape = [1], mode = #arts.mode<inout>, ownerDimCount = 1 : i64, validExtents = [16]}> : (memref<64xf32>) -> ()
      memref.store %value, %cast[%i] : memref<64xf32>
    }
  } {physicalOwnerDims = [0], physicalBlockShape = [16]}
  return
}
