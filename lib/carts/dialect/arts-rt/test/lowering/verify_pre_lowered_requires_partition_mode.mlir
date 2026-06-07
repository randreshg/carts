// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-pre-lowered)' 2>&1 | %FileCheck %s

module {
  func.func @missing_partition_mode() {
    %route = arith.constant -1 : i32
    %c1 = arith.constant 1 : index
    // CHECK: error: 'arts.db_alloc' op missing required partition_mode before ABI lowering
    %guid, %ptr = arts.db_alloc[<out>, <heap>, <write>] route(%route : i32) sizes[%c1] elementType(f64) elementSizes[%c1] : (memref<?xi64>, memref<?xmemref<?xf64>>)
    return
  }
}
