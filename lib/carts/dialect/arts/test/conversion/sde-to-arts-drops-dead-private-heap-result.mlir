// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not='captures pointer-bearing value' --implicit-check-not=sde.cu_region

// A residual init CU can yield a heap buffer that became dead after SDE storage
// realization. If the only external use is dealloc, ARTS must keep the buffer
// EDT-local instead of capturing the pointer-bearing allocation across the EDT
// boundary.

// CHECK-LABEL: func.func @drops_dead_private_heap_result
// CHECK: arts.edt
// CHECK: memref.alloc() : memref<4x16xf32>
// CHECK: memref.store
// CHECK: memref.dealloc

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @drops_dead_private_heap_result() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %one = arith.constant 1.0 : f32
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c16] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %A = memref.cast %block : memref<?x?xf32> to memref<4x16xf32>

    %dead = sde.cu_region <single> -> (memref<4x16xf32>) {
      "arts.db_access_window"(%A) <{blockHi = [4], blockLo = [0], mode = #arts.mode<out>, ownerDimCount = 1 : i64, validExtents = [16]}> : (memref<4x16xf32>) -> ()
      %scratch = memref.alloc() : memref<4x16xf32>
      scf.for %i = %c0 to %c4 step %c1 {
        memref.store %one, %A[%i, %c0] : memref<4x16xf32>
        memref.store %one, %scratch[%i, %c0] : memref<4x16xf32>
      }
      sde.yield %scratch : memref<4x16xf32>
    }
    memref.dealloc %dead : memref<4x16xf32>
    return
  }
}
