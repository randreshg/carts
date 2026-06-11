// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @cu_work_contains_su() {
    sde.cu_work () {
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
      sde.yield
    }
    return
  }
}

// CHECK: 'sde.su_iterate' op is nested inside an sde.cu_work body
// CHECK: compute units are executable leaves and SU scheduling must be represented outside the CU
