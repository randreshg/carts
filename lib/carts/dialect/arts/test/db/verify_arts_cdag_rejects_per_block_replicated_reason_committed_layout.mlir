// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 | %FileCheck %s

// A reject-reason string alone is not preservation evidence for a committed
// block layout. ARTS-created replicas must carry the real perBlockReplicated
// fact instead of stamping a non-distributed excuse.

// CHECK: carries a committed SDE block layout but is neither realized as a distributed DB nor marked non-distributed

module {
  func.func @per_block_replicated_reason_committed_layout_rejected() {
    %c4 = arith.constant 4 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant -1 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c4] elementType(f64) elementSizes[%c16] {distributed_reject_reason = "per_block_replicated", planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
