// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_plan

// Standalone CU outlining must not capture pointer-bearing global memrefs from
// the parent block; rematerialize the global inside the EDT body.

// CHECK-LABEL: func.func @standalone_cu_rematerializes_global
// CHECK: memref.get_global @timer
// CHECK: arts.edt <sync>
// CHECK-SAME: memref<?xmemref<?x?xf32>>
// CHECK-NOT: memref<1xf32>
// CHECK: ^bb0(
// CHECK-NOT: memref<1xf32>
// CHECK: memref.get_global @timer
// CHECK: memref.load

module {
  memref.global "private" @timer : memref<1xf32> = dense<0.000000e+00>

  func.func @standalone_cu_rematerializes_global() {
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
      %a = memref.load %cast[%c0, %c0] : memref<4x4xf32>
      %b = memref.load %timer[%c0] : memref<1xf32>
      %sum = arith.addf %a, %b : f32
      func.call @observe(%sum) : (f32) -> ()
    }
    return
  }

  func.func private @observe(f32)
}
