// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-parallelize)' \
// RUN:   | %FileCheck %s

module {
  func.func @guarded_copy(%A: memref<8x8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %zero = arith.constant 0.000000e+00 : f32
    %true = arith.constant true
    %B = sde.mu_alloc : memref<8x8xf32>
    scf.for %i = %c0 to %c8 step %c1 {
      %is_edge = arith.cmpi eq, %i, %c0 : index
      %guard = scf.if %is_edge -> (i1) {
        scf.yield %true : i1
      } else {
        %other = arith.cmpi eq, %i, %c8 : index
        scf.yield %other : i1
      }
      scf.for %j = %c0 to %c8 step %c1 {
        scf.if %guard {
          %v = memref.load %A[%i, %j] : memref<8x8xf32>
          memref.store %v, %B[%i, %j] : memref<8x8xf32>
        } else {
          memref.store %zero, %B[%i, %j] : memref<8x8xf32>
        }
      }
    }
    return
  }
}

// CHECK-LABEL: func.func @guarded_copy
// CHECK:       sde.mu_alloc
// CHECK:       sde.su_iterate
// CHECK:         sde.cu_region <single> {
// CHECK:           scf.if
// CHECK:           scf.for
// CHECK:             memref.load
// CHECK:             memref.store
