// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s
// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @direct_work_in_distribute() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    sde.su_distribute <blocked> {
      %unused = arith.addi %c0, %c1 : index
    }
    return
  }
}

// CHECK: directly inside an sde.su_distribute body
// CHECK: distribution wrappers may contain only nested SUs, sde.redist, or sde.su_barrier
