// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-raise-to-sde)' 2>&1 | %FileCheck %s

// Per-axis emission: all proven-parallel axes become an N-D su_iterate domain;
// the leaf CU holds the store directly (no inner scf.for). Mixed parallel/
// serial nests fail closed until partial-prefix raise is stable.

// CHECK-LABEL: func.func @nd_parallel_domain
// CHECK: sde.su_iterate ({{.*}}) to ({{.*}}, {{.*}}) step ({{.*}}, {{.*}})
// CHECK: sde.cu_region <parallel>
// CHECK-NOT: scf.for
// CHECK: memref.store

// CHECK-LABEL: func.func @reject_unlicensed_float_reduction
// CHECK: scf.for
// CHECK-NOT: sde.su_iterate

func.func @nd_parallel_domain(%A: memref<4x8xf32>, %v: f32) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c8 = arith.constant 8 : index
  scf.for %i = %c0 to %c4 step %c1 {
    scf.for %j = %c0 to %c8 step %c1 {
      memref.store %v, %A[%i, %j] : memref<4x8xf32>
    }
  }
  return
}

func.func @reject_unlicensed_float_reduction(%A: memref<8xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %init = arith.constant 0.0 : f32
  %sum = scf.for %i = %c0 to %c8 step %c1 iter_args(%acc = %init) -> (f32) {
    %v = memref.load %A[%i] : memref<8xf32>
    %next = arith.addf %acc, %v : f32
    scf.yield %next : f32
  }
  memref.store %sum, %A[%c0] : memref<8xf32>
  return
}
