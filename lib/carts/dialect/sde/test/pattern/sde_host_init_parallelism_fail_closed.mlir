// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-parallelize)' \
// RUN:   | %FileCheck %s

module {
  func.func private @timer() -> f64

  func.func @opaque_side_effect_stays_single() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %A = sde.mu_alloc : memref<8xf64>
    scf.for %i = %c0 to %c8 step %c1 {
      %t = func.call @timer() : () -> f64
      memref.store %t, %A[%i] : memref<8xf64>
    }
    return
  }
}

// CHECK-LABEL: func.func @opaque_side_effect_stays_single
// CHECK:       sde.cu_region <single> {
// CHECK-NEXT:    scf.for
// CHECK:           func.call @timer
// CHECK-NOT:     sde.su_iterate
