// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-expose-legal-blocks)' 2>&1 | %FileCheck %s

// The legal-block exposure pass keeps the transitional arrayLayout entry for
// downstream compatibility, but the typed SDE fact must materialize the
// budget-capped legal block grain.

// CHECK-LABEL: func.func @typed_budget_layout
// CHECK: sde.array_layout write array_id(0) owner [0] block [1, 30] logical [5000, 30]
// CHECK: blockShape = [5000, 30]
// CHECK-SAME: budgetBlockShape = [1, 30]

module attributes {"carts.sde.max-block-bytes" = 120 : i64} {
  func.func @typed_budget_layout(%A: memref<5000x30xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c30 = arith.constant 30 : index
    %c5000 = arith.constant 5000 : index
    %zero = arith.constant 0.0 : f32
    sde.su_iterate (%c0, %c0) to (%c5000, %c30) step (%c1, %c1)
        classification(<elementwise>) {
    ^bb0(%elem: index, %q: index):
      sde.array_layout_root write %A : memref<5000x30xf32> array_id(0)
      sde.cu_region <parallel> {
        memref.store %zero, %A[%elem, %q] : memref<5000x30xf32>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [
      {arrayId = 0 : i64, blockShape = [5000, 30],
       kind = "block_parallel", muBlockCount = 1 : i64,
       ownerDims = [0], role = "write"}]}
    return
  }
}
