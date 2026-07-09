// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-no-load-bearing-discardable-attr)' 2>&1 | %FileCheck %s

module {
  func.func @dep_pattern_on_scf_for() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    scf.for %i = %c0 to %c8 step %c1 {
      scf.yield
    } {dep_pattern = #arts.dep_pattern<stencil>}
    return
  }
}

// CHECK: load-bearing discardable attribute '"dep_pattern"'
