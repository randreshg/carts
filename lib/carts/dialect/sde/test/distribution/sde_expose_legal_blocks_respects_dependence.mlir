// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-expose-legal-blocks)' 2>&1 | %FileCheck %s

// SdeExposeLegalBlocks consumes SdeDependence before exposing owner blocks. A
// loop-carried in-place self dependence is not independently blockable just
// because the target memory budget could fit one element per block.

// CHECK-LABEL: func.func @budget_respects_self_dependence
// CHECK: blockShape = [64]
// CHECK-NOT: budgetBlockShape

module attributes {"carts.sde.max-block-bytes" = 4 : i64} {
  func.func @budget_respects_self_dependence(%A: memref<64xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c64 = arith.constant 64 : index
    sde.su_iterate (%c1) to (%c64) step (%c1)
        classification(<stencil>) {
    ^bb0(%i: index):
      sde.array_layout_root write %A : memref<64xf32> array_id(0)
      sde.cu_region <parallel> {
        %prev = arith.subi %i, %c1 : index
        %v = memref.load %A[%prev] : memref<64xf32>
        memref.store %v, %A[%i] : memref<64xf32>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [64],
       kind = "block_parallel", muBlockCount = 1 : i64,
       ownerDims = [0], role = "write"}]}
    return
  }
}
