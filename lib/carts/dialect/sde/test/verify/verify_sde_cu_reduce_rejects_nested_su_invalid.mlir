// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @cu_reduce_contains_su(%acc: f32, %partial: f32) -> f32 {
    %zero = arith.constant 0.0 : f32
    %result = "sde.cu_reduce"(%acc, %partial, %zero) ({
      %c0 = arith.constant 0 : index
      %c1 = arith.constant 1 : index
      %c8 = arith.constant 8 : index
      sde.su_iterate (%c0) to (%c8) step (%c1) {
      ^bb0(%i: index):
        sde.cu_region <single> {
          sde.yield
        }
        sde.yield
      }
      sde.yield %acc : f32
    }) {reduction_kind = #sde.reduction_kind<add>} : (f32, f32, f32) -> f32
    return %result : f32
  }
}

// CHECK: 'sde.su_iterate' op is nested inside an sde.cu_reduce body
// CHECK: compute units are executable leaves and SU scheduling must be represented outside the CU
