// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-accesses-to-arts-deps,verify-arts-objects-only)' 2>&1 \
// RUN:   | %FileCheck %s --implicit-check-not=sde.cu_region

// A result-bearing standalone CU that reads committed distributed DB state must
// still launch on the distributed path. Scalar result wiring must not
// force the compute EDT back to a current-node-only launch.

// CHECK-LABEL: func.func @standalone_cu_result_uses_distributed_route
// CHECK: %{{.*}}, %[[READ_PTR:.*]] = arts.db_acquire[<in>]
// CHECK-SAME: partitioning(<block>)
// CHECK: %{{.*}}, %[[RESULT_PTR:.*]] = arts.db_acquire[<out>]
// CHECK-SAME: partitioning(<coarse>)
// CHECK: arts.edt <sync> <internode> route
// CHECK-SAME: (%[[READ_PTR]], %[[RESULT_PTR]])
// CHECK: ^bb0(%[[DEP:.*]]: memref<?xmemref<?x?xf32>>, %[[RESULT_DEP:.*]]: memref<?xmemref<?xf32>>)
// CHECK: arts.db_ref %[[DEP]]
// CHECK: memref.load
// CHECK: arts.db_ref %[[RESULT_DEP]]
// CHECK: memref.store

module attributes {arts.runtime_total_nodes = 2 : i64, arts.runtime_total_workers = 8 : i64} {
  func.func @standalone_cu_result_uses_distributed_route() -> f32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f32) elementSizes[%c1, %c16] : (memref<?xi64>, memref<?xmemref<?x?xf32>>)
    %block = arts.db_ref %ptr[%c0] : memref<?xmemref<?x?xf32>> -> memref<?x?xf32>
    %cast = memref.cast %block : memref<?x?xf32> to memref<4x16xf32>

    %result = sde.cu_region <single> -> (f32) {
      "arts.db_access_window"(%cast) <{blockHi = [4], blockLo = [0], mode = #arts.mode<in>, ownerDimCount = 1 : i64, validExtents = [16]}> : (memref<4x16xf32>) -> ()
      %v = memref.load %cast[%c0, %c0] : memref<4x16xf32>
      sde.yield %v : f32
    }
    return %result : f32
  }
}
