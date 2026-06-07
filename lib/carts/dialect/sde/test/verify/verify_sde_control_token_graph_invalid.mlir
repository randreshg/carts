// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

// FAIL: a token-carrying sde.su_barrier consumes a completion-token dependency
// graph. SDE has no generic token/dataflow dependency graph; only a plain
// sde.su_barrier is a legal ordering object.
module {
  func.func @control_token_graph() {
    %done = sde.control_token : !sde.completion
    sde.su_barrier(%done : !sde.completion)
    return
  }
}

// CHECK: generic token/dataflow dependency graph
