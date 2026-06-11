// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-atomic-reduction-materialization)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @atomic_reduction_materializes_cu_atomic
// CHECK: sde.su_iterate
// CHECK-NOT: reduction[
// CHECK-NOT: reduction_strategy
// CHECK: sde.cu_region <parallel>
// CHECK: sde.cu_atomic <add>(%arg0, %{{.*}} : memref<1xi64>, i64)
// CHECK-NOT: memref.store %{{.*}}, %arg0[%{{.*}}] : memref<1xi64>

func.func @atomic_reduction_materializes_cu_atomic(
    %sum: memref<1xi64>, %lhs: memref<?xi32>, %rhs: memref<?xi32>, %n: index) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  sde.su_iterate (%c0) to (%n) step (%c1)
      reduction[[#sde.reduction_kind<add>]](%sum : memref<1xi64>)
      reduction_strategy(<atomic>)
      classification(<reduction>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      %lv = memref.load %lhs[%i] : memref<?xi32>
      %le = arith.extsi %lv : i32 to i64
      %rv = memref.load %rhs[%i] : memref<?xi32>
      %re = arith.extsi %rv : i32 to i64
      %partial = arith.muli %le, %re : i64
      %old = memref.load %sum[%c0] : memref<1xi64>
      %new = arith.addi %old, %partial : i64
      memref.store %new, %sum[%c0] : memref<1xi64>
      sde.yield
    }
  } {pattern = #sde.pattern<reduction>}
  return
}
