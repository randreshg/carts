// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-memory-unit-realization)' 2>&1 | %FileCheck %s

// When one producer CU yields multiple aliases for the same storage root, SDE
// must realize one MU identity for all source-visible aliases.

// CHECK-LABEL: func.func @cu_result_aliases_realize_same_mu
// CHECK: %[[MU:.*]] = sde.mu_alloc : memref<1xi64>
// CHECK: %[[CAST:.*]] = memref.cast %[[MU]] : memref<1xi64> to memref<?xi64>
// CHECK: %[[PAIR:.*]]:2 = sde.cu_region <single> -> (memref<1xi64>, memref<?xi64>) {
// CHECK:   memref.store %{{.*}}, %[[MU]][%c0] : memref<1xi64>
// CHECK: sde.su_iterate
// CHECK: memref.load %[[CAST]][%c0] : memref<?xi64>
// CHECK: memref.store %{{.*}}, %[[CAST]][%c0] : memref<?xi64>
// CHECK: memref.load %[[MU]][%c0] : memref<1xi64>
// CHECK-NOT: memref.load %[[PAIR]]#0

func.func @cu_result_aliases_realize_same_mu() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %zero = arith.constant 0 : i64
  %one = arith.constant 1 : i64
  %acc:2 = sde.cu_region <single> -> (memref<1xi64>, memref<?xi64>) {
    %raw = memref.alloca() : memref<1xi64>
    %cast = memref.cast %raw : memref<1xi64> to memref<?xi64>
    memref.store %zero, %raw[%c0] : memref<1xi64>
    sde.yield %raw, %cast : memref<1xi64>, memref<?xi64>
  }
  sde.su_iterate (%c0) to (%c8) step (%c1) classification(<reduction>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      %old = memref.load %acc#1[%c0] : memref<?xi64>
      %sum = arith.addi %old, %one : i64
      memref.store %sum, %acc#1[%c0] : memref<?xi64>
      sde.yield
    }
  }
  %result = memref.load %acc#0[%c0] : memref<1xi64>
  func.call @sink(%result) : (i64) -> ()
  return
}

func.func private @sink(i64)
