// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-atomic-reduction-materialization)' 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}every accumulator store in the leaf CU must be a load/add/store update

func.func @atomic_reduction_rejects_unmatched_accumulator_store(
    %sum: memref<1xi64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %one = arith.constant 1 : i64
  sde.su_iterate (%c0) to (%c8) step (%c1)
      reduction[[#sde.reduction_kind<add>]](%sum : memref<1xi64>)
      reduction_strategy(<atomic>)
      classification(<reduction>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      %old = memref.load %sum[%c0] : memref<1xi64>
      %next = arith.addi %old, %one : i64
      memref.store %next, %sum[%c0] : memref<1xi64>
      memref.store %one, %sum[%c0] : memref<1xi64>
      sde.yield
    }
  }
  return
}
