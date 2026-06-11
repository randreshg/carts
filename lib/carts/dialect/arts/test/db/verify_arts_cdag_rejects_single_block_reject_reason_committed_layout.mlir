// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 | %FileCheck %s

// `single_block` is not valid non-distribution evidence for a committed SDE
// block layout. ARTS must realize explicit graph/storage facts or reject it.

// CHECK: carries a committed SDE block layout but is neither realized as a distributed DB nor marked non-distributed

module {
  func.func @single_block_reject_reason_is_not_evidence() {
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c16] {distributed_reject_reason = "single_block", planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
