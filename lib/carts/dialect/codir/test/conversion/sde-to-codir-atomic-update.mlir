// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir)' \
// RUN:   | %FileCheck %s --implicit-check-not=sde.cu_atomic

module {
  func.func @sde_atomic_to_codir(%out: memref<i32>) {
    %tok = sde.mu_token #sde<access_mode<readwrite>> %out : memref<i32> -> !sde.token<memref<i32>>
    sde.cu_work (%tok : !sde.token<memref<i32>>) {
    ^bb0(%dep: memref<i32>):
      %one = arith.constant 1 : i32
      sde.cu_atomic #sde<reduction_kind<add>>(%dep, %one : memref<i32>, i32)
      sde.yield
    }
    return
  }
}

// CHECK-LABEL: func.func @sde_atomic_to_codir
// CHECK: codir.codelet
// CHECK: codir.atomic_add(%{{.*}}, %{{.*}} : memref<i32>, i32)
