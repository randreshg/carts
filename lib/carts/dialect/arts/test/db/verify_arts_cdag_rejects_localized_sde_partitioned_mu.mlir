// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' 2>&1 | %FileCheck %s

// A DB carrying a committed SDE block layout (planOwnerDims +
// planPhysicalBlockShape) must be realized as a distributed DB, or carry
// explicit evidence of an intentional non-distributed home. A planned MU left
// non-distributed with no such evidence is a silent coarsening, which ARTS
// rejects. (local_only / perBlockReplicated / reject-reason allocs are exempt:
// those are recorded non-distributed homes such as all-gather replicas.)

// CHECK: carries a committed SDE block layout but is neither realized as a distributed DB nor marked non-distributed

module {
  func.func @silently_coarsened_sde_partitioned_mu() {
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
