// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func private @consume_dep(!sde.dep)

  func.func @mu_dep_graph(%A: memref<8xi32>) {
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %dep = sde.mu_dep <write> %A : memref<8xi32> -> !sde.dep
    func.call @consume_dep(%dep) : (!sde.dep) -> ()
    sde.cu_task {
      memref.store %c0_i32, %A[%c0] : memref<8xi32>
      sde.yield
    }
    return
  }
}

// CHECK: sde.mu_dep must remain a local declaration
