// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-raise-to-sde)' 2>&1 | %FileCheck %s
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-raise-to-sde,sde-raise-to-sde)' 2>&1 | %FileCheck %s --check-prefix=IDEMPOTENT

// Proven-independent nests are raised to bare su_iterate + cu_region<parallel>
// with zero optional attrs. Raw host loops and cu_region<single> wrappers are
// both eligible; nests already under su_iterate are skipped (re-entrancy).

// CHECK-LABEL: func.func @promote_raw_nest
// CHECK: sde.su_iterate
// CHECK-NOT: nowait
// CHECK-NOT: pattern
// CHECK: sde.cu_region <parallel>
// CHECK: scf.for
// CHECK: memref.store
// CHECK-NOT: sde.cu_region <single>

// CHECK-LABEL: func.func @promote_wrapped_nest
// CHECK: sde.su_iterate
// CHECK: sde.cu_region <parallel>
// CHECK-NOT: sde.cu_region <single>

// IDEMPOTENT-LABEL: func.func @promote_raw_nest
// IDEMPOTENT: sde.cu_region <parallel>
// IDEMPOTENT-NOT: sde.cu_region <single>

func.func @promote_raw_nest(%A: memref<8x8xf32>, %v: f32) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  scf.for %i = %c0 to %c8 step %c1 {
    scf.for %j = %c0 to %c8 step %c1 {
      memref.store %v, %A[%i, %j] : memref<8x8xf32>
    }
  }
  return
}

func.func @promote_wrapped_nest(%A: memref<8x8xf32>, %v: f32) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  sde.cu_region <single> {
    scf.for %i = %c0 to %c8 step %c1 {
      scf.for %j = %c0 to %c8 step %c1 {
        memref.store %v, %A[%i, %j] : memref<8x8xf32>
      }
    }
  }
  return
}
