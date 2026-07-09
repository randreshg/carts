// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-no-load-bearing-discardable-attr)'

module {
  func.func @clean() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    scf.for %i = %c0 to %c8 step %c1 {
      scf.yield
    }
    return
  }
}
