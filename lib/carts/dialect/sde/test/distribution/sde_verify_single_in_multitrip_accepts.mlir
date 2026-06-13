// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 | %FileCheck %s

// A single leaf carrying a serial_reason license is accepted (the producer
// asserted its serialism); the discarded-parallelism guard does not fire.
// CHECK-LABEL: func.func @tagged_single
func.func @tagged_single(%A: memref<1024xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %one = arith.constant 1.0 : f64
  sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %one, %A[%i] : memref<1024xf64>
      sde.yield
    } {serialReason = #sde.serial_reason<residual_source>}
  }
  return
}
