// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,sde-parallelize)' \
// RUN:   | %FileCheck %s

module {
  func.func @result_cu_init_loop() -> memref<4xf32> {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %zero = arith.constant 0.000000e+00 : f32
    %A = sde.mu_alloc : memref<4xf32>
    %scratch = memref.alloc() : memref<4xf32>
    scf.for %i = %c0 to %c4 step %c1 {
      memref.store %zero, %A[%i] : memref<4xf32>
    }
    return %scratch : memref<4xf32>
  }
}

// CHECK-LABEL: func.func @result_cu_init_loop
// CHECK:       %[[R:.*]] = sde.cu_region <single> -> (memref<4xf32>) {
// CHECK:         memref.alloc
// CHECK:         sde.yield %{{.*}} : memref<4xf32>
// CHECK:       sde.su_iterate
// CHECK:         sde.cu_region <single> {
// CHECK:           memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<4xf32>
// CHECK:       return %[[R]] : memref<4xf32>
