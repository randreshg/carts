// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.cu_region --implicit-check-not=arts.db_access_plan

// A standalone access-bearing CU is executable work, so the SDE-to-ARTS
// boundary must materialize it as an ARTS EDT with explicit DB dependencies.

// CHECK-LABEL: func.func @standalone_cu_access_becomes_edt_dep
// CHECK: %{{.*}}, %[[ACQ_PTR:.*]] = arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>)
// CHECK: arts.edt <sync> <internode> route
// CHECK-SAME: (%[[ACQ_PTR]])
// CHECK: ^bb0(%[[DEP:.*]]: memref<?xmemref<?x?xf32>>
// CHECK: arts.db_ref %[[DEP]]
// CHECK: memref.load
// CHECK: arts.db_release(%[[DEP]])

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @standalone_cu_access_becomes_edt_dep() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c16] {planOwnerDims = [0], planPhysicalBlockShape = [1, 16]} : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %cast = memref.cast %block : memref<?x?xf32> to memref<4x16xf32>

    sde.cu_region <single> {
      "arts.db_access_plan"(%cast) <{blockHi = [4], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [16]}> : (memref<4x16xf32>) -> ()
      %v = memref.load %cast[%c0, %c0] : memref<4x16xf32>
      func.call @observe(%v) : (f32) -> ()
    }
    return
  }

  func.func private @observe(f32)
}
