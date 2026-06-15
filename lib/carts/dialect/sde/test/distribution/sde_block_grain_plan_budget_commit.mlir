// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-block-grain-plan)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @budget_grain_plan_accepts_budget_candidate
// CHECK: sde.cu_region <parallel>
// CHECK: } {groupBlockCount = [2, 2]}
// CHECK: arrayLayout = [{arrayId = 7 : i64, blockShape = [32, 64]
// CHECK-NOT: budgetBlockShape
// CHECK-SAME: kind = "block_parallel"
// CHECK-SAME: muBlockCount = 16 : i64
// CHECK-SAME: ownerDims = [0, 1]
module attributes {carts.logical_total_localities = 1 : i64, carts.logical_total_workers = 4 : i64} {
  func.func @budget_grain_plan_accepts_budget_candidate() {
    %c0 = arith.constant 0 : index
    %c32 = arith.constant 32 : index
    %c64 = arith.constant 64 : index
    %c128 = arith.constant 128 : index
    %c256 = arith.constant 256 : index
    %cst = arith.constant 1.0 : f32
    %A = sde.mu_alloc : memref<128x256xf32>
    sde.su_iterate (%c0, %c0) to (%c128, %c256) step (%c32, %c64)
        classification(<reduction>) {
    ^bb0(%i: index, %j: index):
      sde.array_layout_root write %A : memref<128x256xf32> array_id(7)
      sde.cu_region <parallel> {
        memref.store %cst, %A[%i, %j] : memref<128x256xf32>
        sde.yield
      }
      sde.yield
    } {arrayLayout = [{arrayId = 7 : i64, blockShape = [64, 128],
         budgetBlockShape = [32, 64], kind = "block_parallel",
         muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}]}
    return
  }
}
