// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

// FAIL: an sde.cu_task carrying an mu_dep dependency graph. SDE has no target
// mu_dep dependency graph; ordering edges belong to CODIR after isolation.
module {
  func.func @mu_dep_graph(%A: memref<8xi32>) {
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %dep = sde.mu_dep <write> %A : memref<8xi32> -> !sde.dep
    sde.cu_task deps(%dep : !sde.dep) {
      memref.store %c0_i32, %A[%c0] : memref<8xi32>
      sde.yield
    }
    return
  }
}

// CHECK: target SDE dependency graph
