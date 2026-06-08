// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)' \
// RUN:   | %FileCheck %s
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' -o %t.mlir
// RUN: %carts-compile %t.mlir --pass-pipeline='builtin.module(verify-sde)'

module {
  func.func @escape(%A: memref<8xf32>) -> f32 {
    %c0 = arith.constant 0 : index
    %B = sde.mu_alloc : memref<8xf32>
    %v = memref.load %A[%c0] : memref<8xf32>
    return %v : f32
  }
}

// CHECK-LABEL: func.func @escape
// CHECK: %[[V:.*]] = sde.cu_region <single> -> (f32) {
// CHECK:   %[[LOAD:.*]] = memref.load
// CHECK:   sde.yield %[[LOAD]] : f32
// CHECK: }
// CHECK: return %[[V]] : f32
