// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 | %FileCheck %s

// Companion guard to the Parallelize.cpp:638 single->parallel promotion: a
// source-compute cu_region<single> directly inside a multi-trip su_iterate with
// no serial_reason license is the regression shape and is rejected.
// CHECK: no serial_reason license
func.func @untagged_single_in_multitrip(%A: memref<1024xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %one = arith.constant 1.0 : f64
  sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %one, %A[%i] : memref<1024xf64>
      sde.yield
    }
  }
  return
}
