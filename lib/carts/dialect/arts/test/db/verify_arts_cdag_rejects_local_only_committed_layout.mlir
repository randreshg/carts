// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 | %FileCheck %s

// A committed SDE block layout cannot be preserved by stamping local_only.

// CHECK: carries a committed SDE block layout but is neither realized as a distributed DB nor marked non-distributed

module {
  func.func @local_only_committed_layout_rejected() {
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c16] {local_only, planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
