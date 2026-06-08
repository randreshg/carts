// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

// FAIL: sde.cu_region bodies must use sde.yield as the region terminator.
module {
  func.func @cu_region_bad_terminator() {
    sde.cu_region <parallel> {
      llvm.unreachable
    }
    return
  }
}

// CHECK: expects body to terminate with sde.yield
