// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps)' 2>&1 | %FileCheck %s

// A standalone CU may clone read-only global loads inside an EDT, but
// writable global state must be represented as an explicit dependency.

// CHECK: writes or escapes a cloned read-only memref.global inside a standalone CU

module {
  memref.global "private" @timer : memref<1xf32> = dense<0.000000e+00>

  func.func @rejects_writable_cloned_global() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %route = arith.constant -1 : i32
    %timer = memref.get_global @timer : memref<1xf32>

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c4] {planOwnerDims = [0], planPhysicalBlockShape = [1, 4]} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %cast = memref.cast %block : memref<?x?xf32> to memref<4x4xf32>

    sde.cu_region <single> {
      "arts.db_access_plan"(%cast) <{blockHi = [4], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [4]}> : (memref<4x4xf32>) -> ()
      %value = memref.load %cast[%c0, %c0] : memref<4x4xf32>
      memref.store %value, %timer[%c0] : memref<1xf32>
    }
    return
  }
}
