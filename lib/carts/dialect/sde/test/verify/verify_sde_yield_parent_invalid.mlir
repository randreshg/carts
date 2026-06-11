// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 \
// RUN:   | %FileCheck %s

module {
  func.func @yield_in_non_sde_region(%flag: i1) {
    scf.if %flag {
      sde.yield
    }
    return
  }
}

// CHECK: 'sde.yield' op expects parent op to be one of
