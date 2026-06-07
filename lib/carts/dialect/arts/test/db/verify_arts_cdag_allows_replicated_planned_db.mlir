// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-arts-cdag)' | %FileCheck %s

// A planned DB may legitimately stay non-distributed when it carries explicit
// evidence of an intentional non-distributed home, such as an all-gather
// replica (perBlockReplicated) materialized by contraction/reduction lowering.
// verify-arts-cdag must accept it, not flag a coarsening.

// CHECK-LABEL: func.func @replicated_planned_db_is_allowed
// CHECK: arts.db_alloc
// CHECK-SAME: perBlockReplicated

module {
  func.func @replicated_planned_db_is_allowed() {
    %c8 = arith.constant 8 : index
    %c16 = arith.constant 16 : index
    %route = arith.constant 0 : i32

    %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>, <block>] route(%route : i32) sizes[%c8] elementType(f64) elementSizes[%c16] {local_only, perBlockReplicated, planOwnerDims = [0], planPhysicalBlockShape = [16]} : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
